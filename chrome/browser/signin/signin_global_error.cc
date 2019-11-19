// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_global_error.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/signin/signin_promo.h"
#endif

SigninGlobalError::SigninGlobalError(
    SigninErrorController* error_controller,
    Profile* profile)
    : profile_(profile),
      error_controller_(error_controller) {
  error_controller_->AddObserver(this);
}

SigninGlobalError::~SigninGlobalError() {
  DCHECK(!error_controller_)
      << "SigninGlobalError::Shutdown() was not called";
}

bool SigninGlobalError::HasError() {
  return HasMenuItem();
}

void SigninGlobalError::Shutdown() {
  error_controller_->RemoveObserver(this);
  error_controller_ = NULL;
}

bool SigninGlobalError::HasMenuItem() {
  return error_controller_->HasError();
}

int SigninGlobalError::MenuItemCommandID() {
  return IDC_SHOW_SIGNIN_ERROR;
}

base::string16 SigninGlobalError::MenuItemLabel() {
  // Notify the user if there's an auth error the user should know about.
  if (error_controller_->HasError())
    return l10n_util::GetStringUTF16(IDS_SYNC_SIGN_IN_ERROR_WRENCH_MENU_ITEM);
  return base::string16();
}

void SigninGlobalError::ExecuteMenuItem(Browser* browser) {
#if defined(OS_CHROMEOS)
  if (error_controller_->auth_error().state() !=
      GoogleServiceAuthError::NONE) {
    DVLOG(1) << "Signing out the user to fix a sync error.";
    // TODO(beng): seems like this could just call chrome::AttemptUserExit().
    chrome::ExecuteCommand(browser, IDC_EXIT);
    return;
  }
#endif

  // Global errors don't show up in the wrench menu on mobile.
#if !defined(OS_ANDROID)
  LoginUIService* login_ui = LoginUIServiceFactory::GetForProfile(profile_);
  if (login_ui->current_login_ui()) {
    login_ui->current_login_ui()->FocusUI();
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("Signin.Reauth",
                            signin_metrics::HISTOGRAM_REAUTH_SHOWN,
                            signin_metrics::HISTOGRAM_REAUTH_MAX);
  browser->window()->ShowAvatarBubbleFromAvatarButton(
      BrowserWindow::AVATAR_BUBBLE_MODE_REAUTH,
      signin_metrics::AccessPoint::ACCESS_POINT_MENU, false);
#endif
}

bool SigninGlobalError::HasBubbleView() {
  return !GetBubbleViewMessages().empty();
}

base::string16 SigninGlobalError::GetBubbleViewTitle() {
  return l10n_util::GetStringUTF16(IDS_SIGNIN_ERROR_BUBBLE_VIEW_TITLE);
}

std::vector<base::string16> SigninGlobalError::GetBubbleViewMessages() {
  std::vector<base::string16> messages;

  // If the user isn't signed in, no need to display an error bubble.
  auto* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile_);
  if (identity_manager && !identity_manager->HasPrimaryAccount())
    return messages;

  if (!error_controller_->HasError())
    return messages;

  switch (error_controller_->auth_error().state()) {
    // TODO(rogerta): use account id in error messages.

    // User credentials are invalid (bad acct, etc).
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::SERVICE_ERROR:
      messages.push_back(l10n_util::GetStringUTF16(
          IDS_SYNC_SIGN_IN_ERROR_BUBBLE_VIEW_MESSAGE));
      break;

    // Sync service is not available for this account's domain.
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      messages.push_back(l10n_util::GetStringUTF16(
          IDS_SYNC_UNAVAILABLE_ERROR_BUBBLE_VIEW_MESSAGE));
      break;

    // Generic message for "other" errors.
    default:
      messages.push_back(l10n_util::GetStringUTF16(
          IDS_SYNC_OTHER_SIGN_IN_ERROR_BUBBLE_VIEW_MESSAGE));
  }
  return messages;
}

base::string16 SigninGlobalError::GetBubbleViewAcceptButtonLabel() {
  // If the auth service is unavailable, don't give the user the option to try
  // signing in again.
  if (error_controller_->auth_error().state() ==
      GoogleServiceAuthError::SERVICE_UNAVAILABLE) {
    return l10n_util::GetStringUTF16(
        IDS_SYNC_UNAVAILABLE_ERROR_BUBBLE_VIEW_ACCEPT);
  } else {
    return l10n_util::GetStringUTF16(IDS_SYNC_SIGN_IN_ERROR_BUBBLE_VIEW_ACCEPT);
  }
}

base::string16 SigninGlobalError::GetBubbleViewCancelButtonLabel() {
  return base::string16();
}

void SigninGlobalError::OnBubbleViewDidClose(Browser* browser) {
}

void SigninGlobalError::BubbleViewAcceptButtonPressed(Browser* browser) {
  ExecuteMenuItem(browser);
}

void SigninGlobalError::BubbleViewCancelButtonPressed(Browser* browser) {
  NOTREACHED();
}

void SigninGlobalError::OnErrorChanged() {
  GlobalErrorServiceFactory::GetForProfile(profile_)->NotifyErrorsChanged();
}
