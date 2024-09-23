// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/chrome_desktop_report_request_helper.h"

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"

#if BUILDFLAG(IS_WIN)
// Windows include must be first for the code to compile.
// clang-format off
#include <windows.h>
#include <dpapi.h>
// clang-format on

#include "base/win/registry.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/keychain_data_helper_mac.h"
#include "crypto/apple_keychain.h"
#endif

namespace extensions {
namespace {

#if BUILDFLAG(IS_WIN)
const wchar_t kDefaultRegistryPath[] =
    L"SOFTWARE\\Google\\Endpoint Verification";
const wchar_t kValueName[] = L"Safe Storage";

LONG ReadEncryptedSecret(std::string* encrypted_secret) {
  base::win::RegKey key;
  constexpr DWORD kMaxRawSize = 1024;
  char raw_data[kMaxRawSize];
  DWORD raw_data_size = kMaxRawSize;
  DWORD raw_type;
  encrypted_secret->clear();
  LONG result = key.Open(HKEY_CURRENT_USER, kDefaultRegistryPath, KEY_READ);
  if (result != ERROR_SUCCESS)
    return result;
  result = key.ReadValue(kValueName, raw_data, &raw_data_size, &raw_type);
  if (result != ERROR_SUCCESS)
    return result;
  if (raw_type != REG_BINARY) {
    key.DeleteValue(kValueName);
    return ERROR_INVALID_DATATYPE;
  }
  encrypted_secret->insert(0, raw_data, raw_data_size);
  return ERROR_SUCCESS;
}

// Encrypts the |plaintext| and write the result in |cyphertext|. This
// function was taken from os_crypt/os_crypt_win.cc (Chromium).
LONG EncryptString(const std::string& plaintext, std::string* ciphertext) {
  DATA_BLOB input;
  input.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(plaintext.data()));
  input.cbData = static_cast<DWORD>(plaintext.length());
  ciphertext->clear();

  DATA_BLOB output;
  BOOL result = ::CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr,
                                   0, &output);
  if (!result)
    return ::GetLastError();

  // this does a copy
  ciphertext->assign(reinterpret_cast<std::string::value_type*>(output.pbData),
                     output.cbData);

  LocalFree(output.pbData);
  return ERROR_SUCCESS;
}

// Decrypts the |cyphertext| and write the result in |plaintext|. This
// function was taken from os_crypt/os_crypt_win.cc (Chromium).
LONG DecryptString(const std::string& ciphertext, std::string* plaintext) {
  DATA_BLOB input;
  input.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(ciphertext.data()));
  input.cbData = static_cast<DWORD>(ciphertext.length());
  plaintext->clear();

  DATA_BLOB output;
  BOOL result =
      ::CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0,
                           &output);
  if (!result)
    return ::GetLastError();

  plaintext->assign(reinterpret_cast<char*>(output.pbData), output.cbData);
  LocalFree(output.pbData);
  return ERROR_SUCCESS;
}

LONG CreateRandomSecret(std::string* secret) {
  // Generate a password with 128 bits of randomness.
  const int kBytes = 128 / 8;
  std::string generated_secret =
      base::Base64Encode(base::RandBytesAsVector(kBytes));

  std::string encrypted_secret;
  LONG result = EncryptString(generated_secret, &encrypted_secret);
  if (result != ERROR_SUCCESS)
    return result;

  base::win::RegKey key;
  result = key.Create(HKEY_CURRENT_USER, kDefaultRegistryPath, KEY_WRITE);
  if (result != ERROR_SUCCESS)
    return result;
  result = key.WriteValue(kValueName, encrypted_secret.data(),
                          encrypted_secret.size(), REG_BINARY);
  if (result == ERROR_SUCCESS)
    *secret = generated_secret;
  return result;
}

#elif BUILDFLAG(IS_MAC)  // BUILDFLAG(IS_WIN)

constexpr char kServiceName[] = "Endpoint Verification Safe Storage";
constexpr char kAccountName[] = "Endpoint Verification";

// Custom error code used to represent that a keychain is locked. Value was
// chosen semi-randomly (it doesn't represent any currently defined OSStatus).
constexpr int32_t kKeychainLocked = 125000;

bool IsAuthFailedError(OSStatus status) {
  return status == errSecAuthFailed;
}

OSStatus AddRandomPasswordToKeychain(const crypto::AppleKeychain& keychain,
                                     std::string* secret) {
  // Generate a password with 128 bits of randomness.
  const int kBytes = 128 / 8;
  std::string password = base::Base64Encode(base::RandBytesAsVector(kBytes));

  OSStatus status = WriteKeychainItem(kServiceName, kAccountName, password);
  if (status == noErr)
    *secret = password;
  else
    secret->clear();
  return status;
}

int32_t ReadEncryptedSecret(std::string* password, bool force_recreate) {
  password->clear();

  OSStatus status;
  crypto::ScopedKeychainUserInteractionAllowed user_interaction_allowed(
      FALSE, &status);
  if (status != noErr)
    return status;

  crypto::AppleKeychain keychain;
  UInt32 password_length = 0;
  void* password_data = nullptr;
  base::apple::ScopedCFTypeRef<SecKeychainItemRef> item_ref;
  status = keychain.FindGenericPassword(
      strlen(kServiceName), kServiceName, strlen(kAccountName), kAccountName,
      &password_length, &password_data, item_ref.InitializeInto());
  if (status == noErr) {
    *password = std::string(static_cast<char*>(password_data), password_length);
    keychain.ItemFreeContent(password_data);
    return status;
  }

  bool was_auth_error = IsAuthFailedError(status);
  bool was_item_not_found = status == errSecItemNotFound;

  if ((was_auth_error || force_recreate) && !was_item_not_found) {
    // If the item is present but can't be read:
    // - Verify that the item's keychain is unlocked,
    // - Then try to delete it,
    // - Then recreate the item.
    // If any of those steps fail don't try to proceed any further.
    item_ref.reset();
    OSStatus exists_status = keychain.FindGenericPassword(
        strlen(kServiceName), kServiceName, strlen(kAccountName), kAccountName,
        nullptr, nullptr, item_ref.InitializeInto());
    if (exists_status != noErr) {
      return exists_status;
    }

    // Try to see if the failure is due to the keychain being locked.
    if (was_auth_error) {
      bool unlocked;
      OSStatus keychain_status =
          VerifyKeychainForItemUnlocked(item_ref.get(), &unlocked);
      if (keychain_status != noErr) {
        // Failed to get keychain status.
        return keychain_status;
      }
      if (!unlocked) {
        return kKeychainLocked;
      }
    }

    if (force_recreate) {
      status = keychain.ItemDelete(item_ref.get());
      if (status != noErr) {
        return status;
      }
    }
  }

  if (was_item_not_found || force_recreate) {
    // Add the random password to the default keychain.
    status = AddRandomPasswordToKeychain(keychain, password);

    // If add failed, check whether the default keychain is locked. If so,
    // return the custom status code.
    if (IsAuthFailedError(status)) {
      bool unlocked;
      OSStatus keychain_status = VerifyDefaultKeychainUnlocked(&unlocked);
      if (keychain_status != noErr) {
        // Failed to get keychain status.
        return keychain_status;
      }
      if (!unlocked) {
        return kKeychainLocked;
      }
    }
  }
  return status;
}

#endif  // BUILDFLAG(IS_MAC)

base::FilePath* GetEndpointVerificationDirOverride() {
  static base::NoDestructor<base::FilePath> dir_override;
  return dir_override.get();
}

// Returns "AppData\Local\Google\Endpoint Verification".
base::FilePath GetEndpointVerificationDir() {
  base::FilePath path;
  if (!GetEndpointVerificationDirOverride()->empty())
    return *GetEndpointVerificationDirOverride();

  bool got_path = false;
#if BUILDFLAG(IS_WIN)
  got_path = base::PathService::Get(base::DIR_LOCAL_APP_DATA, &path);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  path = base::nix::GetXDGDirectory(env.get(), base::nix::kXdgConfigHomeEnvVar,
                                    base::nix::kDotConfigDir);
  got_path = !path.empty();
#elif BUILDFLAG(IS_MAC)
  got_path = base::PathService::Get(base::DIR_APP_DATA, &path);
#endif
  if (!got_path)
    return path;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  path = path.AppendASCII("google");
#else
  path = path.AppendASCII("Google");
#endif
  path = path.AppendASCII("Endpoint Verification");
  return path;
}

}  // namespace

// Sets the path used to store Endpoint Verification data for tests.
void OverrideEndpointVerificationDirForTesting(const base::FilePath& path) {
  *GetEndpointVerificationDirOverride() = path;
}

void StoreDeviceData(const std::string& id,
                     const std::optional<std::vector<uint8_t>> data,
                     base::OnceCallback<void(bool)> callback) {
  base::FilePath data_file = GetEndpointVerificationDir();
  if (data_file.empty()) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(pastarmovj): Make sure the resulting path is still a direct file or
  // subdir+file of the EV folder.
  data_file = data_file.AppendASCII(id);

  bool success = false;
  if (data) {
    // Ensure the directory exists.
    success = base::CreateDirectory(data_file.DirName());
    if (!success) {
      LOG(ERROR) << "Could not create directory: "
                 << data_file.DirName().LossyDisplayName();
      std::move(callback).Run(false);
      return;
    }

    base::FilePath tmp_path;
    success = base::CreateTemporaryFileInDir(data_file.DirName(), &tmp_path);
    if (!success) {
      LOG(ERROR) << "Could not open file for writing: "
                 << tmp_path.LossyDisplayName();
      std::move(callback).Run(false);
      return;
    }

    base::WriteFile(tmp_path, *data);
    success = base::Move(tmp_path, data_file);
  } else {
    // Not passing a second parameter means clear the data sored under |id|.
    success = base::DeleteFile(data_file);
    if (base::IsDirectoryEmpty(data_file.DirName()))
      base::DeleteFile(data_file.DirName());
  }

  std::move(callback).Run(success);
}

void RetrieveDeviceData(
    const std::string& id,
    base::OnceCallback<void(const std::string&, RetrieveDeviceDataStatus)>
        callback) {
  base::FilePath data_file = GetEndpointVerificationDir();
  if (data_file.empty()) {
    std::move(callback).Run("",
                            RetrieveDeviceDataStatus::kDataDirectoryUnknown);
    return;
  }
  data_file = data_file.AppendASCII(id);
  // If the file does not exist don't treat this as an error rather return an
  // empty string.
  if (!base::PathExists(data_file)) {
    std::move(callback).Run("", RetrieveDeviceDataStatus::kDataRecordNotFound);
    return;
  }
  std::string data;
  // ReadFileToString does not permit traversal with .. so this is guaranteed to
  // be a descendant of the data directory up to links created outside of
  // Chrome.
  bool result = base::ReadFileToString(data_file, &data);

  std::move(callback).Run(
      data, result ? RetrieveDeviceDataStatus::kSuccess
                   : RetrieveDeviceDataStatus::kDataRecordRetrievalError);
}

void RetrieveDeviceSecret(
    bool force_recreate,
    base::OnceCallback<void(const std::string&, int32_t)> callback) {
  std::string secret;
#if BUILDFLAG(IS_WIN)
  std::string encrypted_secret;
  LONG result = ReadEncryptedSecret(&encrypted_secret);
  if (result == ERROR_FILE_NOT_FOUND)
    result = CreateRandomSecret(&secret);
  else if (result == ERROR_SUCCESS)
    result = DecryptString(encrypted_secret, &secret);
  // If something failed above [re]try creating the secret if forced.
  if (result != ERROR_SUCCESS && force_recreate)
    result = CreateRandomSecret(&secret);
#elif BUILDFLAG(IS_MAC)
  int32_t result = ReadEncryptedSecret(&secret, force_recreate);
#else
  int32_t result = -1;  // Anything but 0 is a failure.
#endif
  std::move(callback).Run(secret, static_cast<int32_t>(result));
}

}  // namespace extensions
