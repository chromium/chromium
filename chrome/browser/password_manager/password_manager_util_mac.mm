// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_util_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <Security/Authorization.h>

#include "base/mac/authorization_util.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_authorizationref.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

NSString* UserAuthenticationRightName() {
  // The authentication right name is of the form
  // `org.chromium.Chromium.access-passwords` or
  // `com.google.Chrome.access-passwords`.
  return [[base::mac::MainBundle() bundleIdentifier]
      stringByAppendingString:@".access-passwords"];
}

bool EnsureAuthorizationRightExists() {
  NSString* rightName = UserAuthenticationRightName();
  // If the authorization right already exists there is nothing to do.
  if (AuthorizationRightGet(rightName.UTF8String, nullptr) ==
      errAuthorizationSuccess) {
    return true;
  }

  // The authorization right does not exist so create it.
  base::mac::ScopedAuthorizationRef authorization =
      base::mac::CreateAuthorization();
  if (!authorization) {
    return false;
  }

  // Create a right which requires that the user authenticate as the session
  // owner. The prompt must be specified each time the right is requested.
  OSStatus status =
      AuthorizationRightSet(authorization, rightName.UTF8String,
                            CFSTR(kAuthorizationRuleAuthenticateAsSessionUser),
                            nullptr, nullptr, nullptr);
  if (status != errAuthorizationSuccess) {
    OSSTATUS_LOG(ERROR, status) << "AuthorizationRightSet";
    return false;
  }

  return true;
}

}  // namespace

namespace password_manager_util_mac {

bool AuthenticateUser(password_manager::ReauthPurpose purpose) {
  if (!EnsureAuthorizationRightExists()) {
    return false;
  }

  NSString* rightName = UserAuthenticationRightName();
  AuthorizationItem right_items[] = {{rightName.UTF8String, 0, nullptr, 0}};
  AuthorizationRights rights = {std::size(right_items), right_items};

  NSString* prompt;
  switch (purpose) {
    case password_manager::ReauthPurpose::VIEW_PASSWORD:
      prompt = l10n_util::GetNSString(IDS_PASSWORDS_PAGE_AUTHENTICATION_PROMPT);
      break;
    case password_manager::ReauthPurpose::COPY_PASSWORD:
      prompt =
          l10n_util::GetNSString(IDS_PASSWORDS_PAGE_COPY_AUTHENTICATION_PROMPT);
      break;
    case password_manager::ReauthPurpose::EDIT_PASSWORD:
      prompt =
          l10n_util::GetNSString(IDS_PASSWORDS_PAGE_EDIT_AUTHENTICATION_PROMPT);
      break;
    case password_manager::ReauthPurpose::EXPORT:
      prompt = l10n_util::GetNSString(
          IDS_PASSWORDS_PAGE_EXPORT_AUTHENTICATION_PROMPT);
      break;
  }

  // Pass kAuthorizationFlagDestroyRights to prevent the OS from saving the
  // authorization and not prompting the user when future requests are made.
  base::mac::ScopedAuthorizationRef authorization =
      base::mac::GetAuthorizationRightsWithPrompt(
          &rights, base::mac::NSToCFCast(prompt),
          kAuthorizationFlagDestroyRights);
  return static_cast<bool>(authorization);
}

std::u16string GetMessageForBiometricLoginPrompt(
    password_manager::ReauthPurpose purpose) {
  // Depending on the `purpose` different message will be returned.
  switch (purpose) {
    case password_manager::ReauthPurpose::VIEW_PASSWORD:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
    case password_manager::ReauthPurpose::COPY_PASSWORD:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_COPY_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
    case password_manager::ReauthPurpose::EDIT_PASSWORD:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_EDIT_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
    case password_manager::ReauthPurpose::EXPORT:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_EXPORT_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
  }
}

}  // namespace password_manager_util_mac
