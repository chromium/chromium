// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_util_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <Security/Authorization.h>

#include "base/mac/authorization_util.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_authorizationref.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace password_manager_util_mac {

bool AuthenticateUser(password_manager::ReauthPurpose purpose) {
  // Use the system-defined "system.login.screensaver" access right rather than
  // creating our own. The screensaver does exactly the same check we need --
  // verifying whether the legitimate session user is present. If we needed to
  // create a separate access right, we would have to define it with the
  // AuthorizationDB, using the flag
  // kAuthorizationRuleAuthenticateAsSessionUser, to ensure that the session
  // user password, as opposed to an admin's password, is required.
  AuthorizationItem right_items[] = {
      {"system.login.screensaver", 0, nullptr, 0}};
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
  return authorization.get() != nullptr;
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
