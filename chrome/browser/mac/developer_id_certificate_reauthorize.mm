// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/developer_id_certificate_reauthorize.h"

#import <Foundation/Foundation.h>
#include <Security/Security.h>
#include <crt_externs.h>
#include <errno.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/scoped_generic.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "chrome/common/chrome_version.h"
#include "components/os_crypt/keychain_password_mac.h"
#include "crypto/apple_keychain.h"

namespace chrome {

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

struct VectorScramblerTraits {
  static std::vector<uint8_t>* InvalidValue() { return nullptr; }

  static void Free(std::vector<uint8_t>* buf) {
    memset(buf->data(), 0, buf->size());
    delete buf;
  }
};

using ScopedVectorScrambler =
    base::ScopedGeneric<std::vector<uint8_t>*, VectorScramblerTraits>;

// Reauthorizes the Safe Storage keychain item, which protects the randomly
// generated password that encrypts the user's saved passwords. This reads the
// Keychain item, deletes it, and adds it back to the Keychain. This works
// because the Keychain uses the code's designated requirement at the time a
// Keychain item is written to build the ACL consulted when subsequently reading
// it. The application is now signed with a designated requirement that
// tolerates both old and new certificates.
bool KeychainReauthorize() {
  // Don't allow user interaction. If the running code doesn't have access to
  // the existing Keychain item, it's pointless to prompt now just to
  // reauthorize it. Just let the OS prompt the user when the secret is actually
  // needed.
  crypto::ScopedKeychainUserInteractionAllowed
      keychain_user_interaction_allowed(FALSE);

  const std::string& keychain_service_name = KeychainPassword::GetServiceName();
  const std::string& keychain_account_name = KeychainPassword::GetAccountName();

  crypto::AppleKeychain keychain;
  UInt32 password_length = 0;
  void* password_data = nullptr;
  base::ScopedCFTypeRef<SecKeychainItemRef> keychain_item;
  OSStatus status = keychain.FindGenericPassword(
      keychain_service_name.size(), keychain_service_name.c_str(),
      keychain_account_name.size(), keychain_account_name.c_str(),
      &password_length, &password_data, keychain_item.InitializeInto());

  // As in components/os_crypt/keychain_password_mac.mm, this string is
  // user-facing, but is also used as the key to access data in the Keychain, so
  // DO NOT LOCALIZE.
  const std::string backup_service_name = keychain_service_name + " Backup";
  base::ScopedCFTypeRef<SecKeychainItemRef> backup_item;
  if (status != noErr) {
    if (OSStatus backup_status = keychain.FindGenericPassword(
            backup_service_name.size(), backup_service_name.data(),
            keychain_account_name.size(), keychain_account_name.c_str(),
            &password_length, &password_data, backup_item.InitializeInto());
        backup_status != noErr) {
      // Neither the main nor backup item exists. It's not possible to continue.
      OSSTATUS_LOG(ERROR, status) << "SecKeychainFindGenericPassword (main)";
      OSSTATUS_LOG(ERROR, backup_status)
          << "SecKeychainFindGenericPassword (backup)";
      return false;
    }

    // The main item was absent, but the backup was present. Proceed with it.
    OSSTATUS_LOG(WARNING, status) << "SecKeychainFindGenericPassword (main)";
  }

  // A password was retrieved, either from the main or backup item. Store it in
  // the self-scrambling |password|, and release the copy decrypted from the
  // Keychain item.
  ScopedVectorScrambler password;
  password.reset(new std::vector<uint8_t>(
      static_cast<uint8_t*>(password_data),
      static_cast<uint8_t*>(password_data) + password_length));
  memset(password_data, 0, password_length);
  keychain.ItemFreeContent(password_data);

  if (keychain_item) {
    // If the main item was obtained, attempt to save the backup item, as a
    // non-critical step. If it fails, continue on to reauthorizing the main
    // item anyway.
    status = keychain.AddGenericPassword(
        backup_service_name.size(), backup_service_name.data(),
        keychain_account_name.size(), keychain_account_name.c_str(),
        password.get()->size(), password.get()->data(),
        backup_item.InitializeInto());
    OSSTATUS_LOG_IF(WARNING, status != noErr, status)
        << "SecKeychainAddGenericPassword (backup)";

    status = keychain.ItemDelete(keychain_item);
    OSSTATUS_LOG_IF(WARNING, status != noErr, status)
        << "SecKeychainItemDelete (main)";
  }

  // Store a new copy of the main item, with the password that was retrieved.
  // The designated requirement associated with the currently-running code will
  // be used to form the access requirement for the new item.
  status = keychain.AddGenericPassword(
      keychain_service_name.size(), keychain_service_name.c_str(),
      keychain_account_name.size(), keychain_account_name.c_str(),
      password.get()->size(), password.get()->data(), nullptr);
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "SecKeychainAddGenericPassword (main)";
    return false;
  }

  if (!backup_item) {
    // Even if no backup entry was used thus far, attempt to locate one. It may
    // be present if a previous reauthorization attempt crashed after writing
    // the backup entry and before deleting the main entry. Don't log anything
    // because the backup entry is presumed not to exist.
    keychain.FindGenericPassword(
        backup_service_name.size(), backup_service_name.data(),
        keychain_account_name.size(), keychain_account_name.c_str(), 0, nullptr,
        backup_item.InitializeInto());
  }

  // If it exists, remove the backup item.
  if (backup_item) {
    status = keychain.ItemDelete(backup_item);
    OSSTATUS_LOG_IF(WARNING, status != noErr, status)
        << "SecKeychainItemDelete (backup)";
  }

  return true;
}

NSUserDefaults* UserDefaults() {
  // This returns an NSUserDefaults that is appropriate for tracking whether
  // or not Developer ID certifiate reauthorization has occurred or been
  // attempted.
  //
  // There is just one safe storage Keychain item, not one per profile or one
  // per channel. The scoping of the preference used to track reauthorization
  // should match that of the Keychain item itself.
  //
  // If a regular preference in the Chrome profile was used here,
  // reauthorization would be tracked per profile.
  //
  // If +[NSUserDefaults standardUserDefaults] was used, it would return a
  // NSUserDefaults object for a preference domain matching the running process'
  // bundle ID (approximately equivalent to the product's channel for these
  // purposes). The reauthorization stub executable would be provided with an
  // NSUserDefaults object using a preference domain specific to itself. This
  // would result in reauthorization being tracked per channel (and stub).
  //
  // In order to get all channels and the stub executable using the same
  // preference, -[NSUserDefaults initWithSuiteName:] is used, specifying a
  // suite name matching the product's unchannelized base bundle ID. To keep
  // things interesting, this method doesn't work when the suite name provided
  // is the same as the running application's own bundle ID, as nil is returned
  // in that case. In that situation, the old standby +[NSUserDefaults
  // standardUserDefaults] sets things up to the desired preference domain as
  // standard, and all others can specify a suite name to arrive at the same
  // preference domain.
  NSString* const kPreferenceBundleID = @MAC_BUNDLE_IDENTIFIER_STRING;
  if ([[base::mac::MainBundle() bundleIdentifier]
          isEqualToString:kPreferenceBundleID]) {
    return [NSUserDefaults standardUserDefaults];
  }
  return [[[NSUserDefaults alloc] initWithSuiteName:kPreferenceBundleID]
      autorelease];
}

NSString* const kPreferenceKeyBase =
    @"DeveloperIDCertificateReauthorizeWinter20212022";
NSString* const kPreferenceKeyAttemptsSuffix = @"Attempts";
NSString* const kPreferenceKeySuccessSuffix = @"Success";

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

void DeveloperIDCertificateReauthorizeInApp() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  NSUserDefaults* user_defaults = UserDefaults();
  NSString* attempts_pref_key =
      [kPreferenceKeyBase stringByAppendingString:kPreferenceKeyAttemptsSuffix];
  int attempts_pref_value = [user_defaults integerForKey:attempts_pref_key];

  constexpr int kMaxAttempts = 2;
  if (attempts_pref_value >= kMaxAttempts) {
    return;
  }

  NSString* success_pref_key =
      [kPreferenceKeyBase stringByAppendingString:kPreferenceKeySuccessSuffix];
  BOOL success_pref_value = [user_defaults boolForKey:success_pref_key];
  if (success_pref_value) {
    return;
  }

  ++attempts_pref_value;
  [user_defaults setInteger:attempts_pref_value forKey:attempts_pref_key];
  [user_defaults synchronize];

  // While the main application is signed with the old certificate but with the
  // new designated requirement, kReauthorizeInApp may be true to perform the
  // reauthorization directly in-process. When the main application's code
  // signing certificate changes, set kReauthorizeInApp to false to cause
  // reauthorization to be performed by the stub executable, which is signed
  // with the old certificate and the new designated requirement, and which will
  // not be re-signed even when the application's code signing certificate
  // changes.
  constexpr bool kReauthorizeInApp = false;

  if constexpr (kReauthorizeInApp) {
    bool success = KeychainReauthorize();
    if (!success) {
      return;
    }

    [user_defaults setBool:YES forKey:success_pref_key];
    [user_defaults synchronize];
  } else {
    base::FilePath reauthorize_executable_path =
        base::mac::FrameworkBundlePath().Append("Helpers").Append(
            "developer_id_certificate_reauthorize");
    std::string identifier =
        base::SysNSStringToUTF8([base::mac::OuterBundle() bundleIdentifier]);

    std::vector<std::string> argv = {reauthorize_executable_path.value(),
                                     identifier};

    // Execution must block waiting for the stub executable to avoid a race
    // between it and the application attempting to access the Keychain item.
    base::LaunchOptions launch_options;
    launch_options.wait = true;
    base::ScopedAllowBlocking allow_blocking;

    base::LaunchProcess(argv, launch_options);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

}  // namespace chrome

__attribute__((visibility("default"))) extern "C" int
DeveloperIDCertificateReauthorizeFromStub(int argc, const char* const* argv) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  @autoreleasepool {
    NSUserDefaults* user_defaults = chrome::UserDefaults();

    NSString* success_pref_key = [chrome::kPreferenceKeyBase
        stringByAppendingString:chrome::kPreferenceKeySuccessSuffix];
    BOOL success_pref_value = [user_defaults boolForKey:success_pref_key];
    if (success_pref_value) {
      return EXIT_SUCCESS;
    }

    bool success = chrome::KeychainReauthorize();
    if (!success) {
      return EXIT_FAILURE;
    }

    [user_defaults setBool:YES forKey:success_pref_key];
    [user_defaults synchronize];

    return EXIT_SUCCESS;
  }
#else
  return EXIT_FAILURE;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
