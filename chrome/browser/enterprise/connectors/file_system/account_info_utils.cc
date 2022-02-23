// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/account_info_utils.h"

#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_flow.h"
#include "components/os_crypt/os_crypt.h"
#include "components/prefs/pref_registry_simple.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace {

// Templates for the profile preferences paths to store account related
// information, per service provider.
// OAuth2 tokens:
constexpr char kAccessTokenPrefPathTemplate[] =
    "enterprise_connectors.file_system.%s.access_token";
constexpr char kRefreshTokenPrefPathTemplate[] =
    "enterprise_connectors.file_system.%s.refresh_token";
// Default upload folder information:
constexpr char kUploadFolderIdPrefPathTemplate[] =
    "enterprise_connectors.file_system.%s.folder.id";
constexpr char kUploadFolderNamePrefPathTemplate[] =
    "enterprise_connectors.file_system.%s.folder.name";
// Account information to be displayed on the settings page:
constexpr char kAccountInfoPrefPathTemplate[] =
    "enterprise_connectors.file_system.%s.account";

constexpr char kUploadFolderDefaultName[] = "ChromeDownloads";

// Encrypt an access or refresh token.
bool EncryptToken(std::string token, std::string* b64_enc_token) {
  std::string enc_token;
  if (!b64_enc_token || !OSCrypt::EncryptString(token, &enc_token)) {
    return false;
  }

  base::Base64Encode(enc_token, b64_enc_token);
  return true;
}

// Encrypt and store an access or refresh token into a profile preference.
bool EncryptAndStoreToken(PrefService* prefs,
                          const std::string& path,
                          std::string token) {
  std::string b64_enc_token;
  if (!prefs || !EncryptToken(token, &b64_enc_token)) {
    return false;
  }

  prefs->SetString(path, b64_enc_token);
  return true;
}

// Retrieve and decrypt an encrypted access or refresh token stored in a profile
// preference.
bool RetrieveAndDecryptToken(PrefService* prefs,
                             const std::string& path,
                             std::string* decrypted_token) {
  if (!prefs || !decrypted_token) {
    return false;
  }
  std::string b64_enc_pref = prefs->GetString(path);
  if (b64_enc_pref.empty()) {
    // See RegisterFileSystemPrefsForServiceProvider().
    *decrypted_token = std::string();
    return true;
  }
  std::string enc_pref;
  return base::Base64Decode(b64_enc_pref, &enc_pref) &&
         OSCrypt::DecryptString(enc_pref, decrypted_token);
}

bool GetToken(PrefService* prefs,
              const std::string& service_provider,
              std::string* token,
              const char* token_pref_path_template) {
  return !token ||
         RetrieveAndDecryptToken(prefs,
                                 base::StringPrintf(token_pref_path_template,
                                                    service_provider.c_str()),
                                 token);
}

std::string GetPrefPath(const char* pref_path_template,
                        const std::string& service_provider) {
  return base::StringPrintf(pref_path_template, service_provider.c_str());
}

}  // namespace

namespace enterprise_connectors {

void RegisterFileSystemPrefsForServiceProvider(
    PrefRegistrySimple* registry,
    const std::string& service_provider) {
  registry->RegisterStringPref(
      GetPrefPath(kUploadFolderIdPrefPathTemplate, service_provider),
      std::string());
  registry->RegisterStringPref(
      GetPrefPath(kUploadFolderNamePrefPathTemplate, service_provider),
      kUploadFolderDefaultName);
  registry->RegisterDictionaryPref(
      GetPrefPath(kAccountInfoPrefPathTemplate, service_provider),
      base::DictionaryValue());
  // On some platforms, encrypting an empty string results in a non-empty
  // ciphertext. Howevver, all the unit tests that use TestingProfile's do not
  // necessarily include OSCryptMocker::SetUp/TearDown(). Therefore, we have to
  // initialize the prefs an empty strings, even though they cannot get
  // decrypted successfully on those platforms. As a result, in
  // RetrieveAndDecryptToken(), we have to check whether the stored pref is
  // empty before we send it in for decryption.
  registry->RegisterStringPref(
      GetPrefPath(kAccessTokenPrefPathTemplate, service_provider),
      std::string());
  registry->RegisterStringPref(
      GetPrefPath(kRefreshTokenPrefPathTemplate, service_provider),
      std::string());
}

bool SetFileSystemToken(PrefService* prefs,
                        const std::string& service_provider,
                        const char token_pref_path_template[],
                        const std::string& token) {
  return EncryptAndStoreToken(
      prefs, GetPrefPath(token_pref_path_template, service_provider.c_str()),
      token);
}

bool SetFileSystemOAuth2Tokens(PrefService* prefs,
                               const std::string& service_provider,
                               const std::string& access_token,
                               const std::string& refresh_token) {
  return SetFileSystemToken(prefs, service_provider,
                            kAccessTokenPrefPathTemplate, access_token) &&
         SetFileSystemToken(prefs, service_provider,
                            kRefreshTokenPrefPathTemplate, refresh_token);
}

bool ClearFileSystemAccessToken(PrefService* prefs,
                                const std::string& service_provider) {
  return SetFileSystemToken(prefs, service_provider,
                            kAccessTokenPrefPathTemplate, std::string());
}

bool ClearFileSystemRefreshToken(PrefService* prefs,
                                 const std::string& service_provider) {
  return SetFileSystemToken(prefs, service_provider,
                            kRefreshTokenPrefPathTemplate, std::string());
}

bool ClearFileSystemOAuth2Tokens(PrefService* prefs,
                                 const std::string& service_provider) {
  return ClearFileSystemAccessToken(prefs, service_provider) &&
         ClearFileSystemRefreshToken(prefs, service_provider);
}

bool GetFileSystemOAuth2Tokens(PrefService* prefs,
                               const std::string& service_provider,
                               std::string* access_token,
                               std::string* refresh_token) {
  return GetToken(prefs, service_provider, access_token,
                  kAccessTokenPrefPathTemplate) &&
         GetToken(prefs, service_provider, refresh_token,
                  kRefreshTokenPrefPathTemplate);
}

void SetDefaultFolder(PrefService* prefs,
                      const std::string& service_provider,
                      std::string folder_id,
                      std::string folder_name) {
  prefs->SetString(
      GetPrefPath(kUploadFolderIdPrefPathTemplate, service_provider),
      folder_id);
  prefs->SetString(
      GetPrefPath(kUploadFolderNamePrefPathTemplate, service_provider),
      folder_name);
}

void ClearDefaultFolder(PrefService* prefs,
                        const std::string& service_provider) {
  SetDefaultFolder(prefs, service_provider, std::string(), std::string());
}

std::string GetDefaultFolderId(PrefService* prefs,
                               const std::string& service_provider) {
  auto folder_id = prefs->GetString(
      GetPrefPath(kUploadFolderIdPrefPathTemplate, service_provider));
  return folder_id;
}

std::string GetDefaultFolderLink(PrefService* prefs,
                                 const std::string& service_provider) {
  auto folder_id = GetDefaultFolderId(prefs, service_provider);
  DCHECK_EQ(service_provider, kFileSystemServiceProviderPrefNameBox);
  std::string url = BoxApiCallFlow::MakeUrlToShowFolder(folder_id).spec();
  DCHECK_EQ(url.empty(), folder_id.empty());
  return url;
}

std::string GetDefaultFolderName(PrefService* prefs,
                                 const std::string& service_provider) {
  std::string name = prefs->GetString(
      GetPrefPath(kUploadFolderNamePrefPathTemplate, service_provider));
  return name.empty() ? kUploadFolderDefaultName : name;
}

std::vector<std::string> GetFileSystemConnectorAccountInfoPrefs(
    const std::string& service_provider) {
  std::vector<std::string> prefs_paths;
  prefs_paths.emplace_back(
      GetPrefPath(kUploadFolderIdPrefPathTemplate, service_provider));
  prefs_paths.emplace_back(
      GetPrefPath(kUploadFolderNamePrefPathTemplate, service_provider));
  prefs_paths.emplace_back(
      GetPrefPath(kAccountInfoPrefPathTemplate, service_provider));
  return prefs_paths;
}

void SetFileSystemAccountInfo(PrefService* prefs,
                              const std::string& service_provider,
                              base::Value account_info) {
  prefs->Set(GetPrefPath(kAccountInfoPrefPathTemplate, service_provider),
             account_info);
}

bool ClearFileSystemAccountInfo(PrefService* prefs,
                                const std::string& service_provider) {
  const auto path = GetPrefPath(kAccountInfoPrefPathTemplate, service_provider);
  if (!prefs->FindPreference(path))
    return false;
  prefs->Set(path, base::DictionaryValue());
  return true;
}

base::Value GetFileSystemAccountInfo(PrefService* prefs,
                                     const std::string& service_provider) {
  const auto path = GetPrefPath(kAccountInfoPrefPathTemplate, service_provider);

  const base::Value* val = prefs->GetDictionary(path);
  DCHECK(val);
  return val->Clone();
}

AccountInfo::AccountInfo() = default;
AccountInfo::~AccountInfo() = default;
AccountInfo::AccountInfo(const AccountInfo& other) = default;

absl::optional<AccountInfo> GetFileSystemAccountInfoFromPrefs(
    const FileSystemSettings& settings,
    PrefService* prefs) {
  const std::string& provider = settings.service_provider;
  base::Value stored_account_info = GetFileSystemAccountInfo(prefs, provider);
  std::string *account_name, *account_login;
  if (stored_account_info.DictEmpty() ||
      !(account_name = stored_account_info.FindStringPath("name")) ||
      !(account_login = stored_account_info.FindStringPath("login"))) {
    return absl::nullopt;
  }

  AccountInfo account_info;
  account_info.account_name = *account_name;
  account_info.account_login = *account_login;
  account_info.folder_name = GetDefaultFolderName(prefs, provider);
  account_info.folder_link = GetDefaultFolderLink(prefs, provider);
  DCHECK(!account_info.account_name.empty());
  DCHECK(!account_info.account_login.empty());
  DCHECK(!account_info.folder_name.empty());
  return absl::make_optional<AccountInfo>(std::move(account_info));
}

}  // namespace enterprise_connectors
