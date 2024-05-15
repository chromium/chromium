// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/password_reuse_controller_android.h"

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/ui/android/safe_browsing/password_reuse_dialog_view_android.h"
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace safe_browsing {

PasswordReuseControllerAndroid::PasswordReuseControllerAndroid(
    content::WebContents* web_contents,
    ChromePasswordProtectionService* service,
    ReusedPasswordAccountType password_type,
    OnWarningDone done_callback)
    : content::WebContentsObserver(web_contents),
      service_(service),
      url_(web_contents->GetLastCommittedURL()),
      password_type_(password_type),
      window_android_(web_contents->GetTopLevelNativeWindow()),
      done_callback_(std::move(done_callback)) {
  modal_construction_start_time_ = base::TimeTicks::Now();

  // |service| can be nullptr in tests
  if (service)
    service_->AddObserver(this);
}

PasswordReuseControllerAndroid::~PasswordReuseControllerAndroid() {
  if (service_)
    service_->RemoveObserver(this);

  dialog_view_.reset();
  LogModalWarningDialogLifetime(modal_construction_start_time_);
}

void PasswordReuseControllerAndroid::ShowDialog() {
  dialog_view_ = std::make_unique<PasswordReuseDialogViewAndroid>(this);

  DCHECK(window_android_);
  dialog_view_->Show(window_android_);
}

void PasswordReuseControllerAndroid::ShowCheckPasswords() {
  if (done_callback_)
    std::move(done_callback_).Run(WarningAction::CHANGE_PASSWORD);

  delete this;
}

void PasswordReuseControllerAndroid::IgnoreDialog() {
  if (done_callback_)
    std::move(done_callback_).Run(WarningAction::IGNORE_WARNING);

  delete this;
}

void PasswordReuseControllerAndroid::CloseDialog() {
  if (done_callback_)
    std::move(done_callback_).Run(WarningAction::CLOSE);

  delete this;
}

std::u16string PasswordReuseControllerAndroid::GetPrimaryButtonText() const {
  if (password_type_.account_type() == ReusedPasswordAccountType::GMAIL &&
      password_type_.is_account_syncing()) {
    return l10n_util::GetStringUTF16(IDS_PAGE_INFO_PROTECT_ACCOUNT_BUTTON);
  }
#if BUILDFLAG(IS_ANDROID)
  // The modal can be shown on automotive, but should not lead users to the
  // GMSCore Password Check UI, as that is not optimized for automotive.
  else if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    return l10n_util::GetStringUTF16(IDS_CLOSE);
  }
#endif
  else if (password_type_.account_type() ==
           ReusedPasswordAccountType::SAVED_PASSWORD) {
    return l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHECK_PASSWORDS_BUTTON);
  }

  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

std::u16string PasswordReuseControllerAndroid::GetSecondaryButtonText() const {
  if (password_type_.account_type() == ReusedPasswordAccountType::GMAIL &&
      password_type_.is_account_syncing()) {
    return l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_IGNORE_PASSWORD_WARNING_BUTTON);
  }
#if BUILDFLAG(IS_ANDROID)
  // The modal can be shown on automotive, but without any call to action as
  // those are not optimized for automotive.
  else if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    return std::u16string();
  }
#endif
  else if (password_type_.account_type() ==
           ReusedPasswordAccountType::SAVED_PASSWORD) {
    return l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_IGNORE_PASSWORD_WARNING_BUTTON);
  }

  return std::u16string();
}

std::u16string PasswordReuseControllerAndroid::GetWarningDetailText() const {
  return service_->GetWarningDetailText(password_type_);
}

std::u16string PasswordReuseControllerAndroid::GetTitle() const {
  if (password_type_.account_type() ==
      ReusedPasswordAccountType::SAVED_PASSWORD) {
    return l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_CHANGE_PASSWORD_SAVED_PASSWORD_SUMMARY);
  }

  return l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_SUMMARY);
}

void PasswordReuseControllerAndroid::OnGaiaPasswordChanged() {
  delete this;
  // Chrome on Android should not be able to capture Gaia password change
  // events.
  NOTREACHED_IN_MIGRATION();
}

void PasswordReuseControllerAndroid::OnMarkingSiteAsLegitimate(
    const GURL& url) {
  if (url_.GetWithEmptyPath() == url.GetWithEmptyPath())
    delete this;
  // Modal dialog on Android is above the screen, this function can't be called.
  NOTREACHED_IN_MIGRATION();
}

void PasswordReuseControllerAndroid::InvokeActionForTesting(
    WarningAction action) {
  CloseDialog();
}

WarningUIType PasswordReuseControllerAndroid::GetObserverType() {
  return WarningUIType::MODAL_DIALOG;
}

void PasswordReuseControllerAndroid::WebContentsDestroyed() {
  delete this;
}

void PasswordReuseControllerAndroid::SetReusedPasswordAccountTypeForTesting(
    ReusedPasswordAccountType password_type) {
  password_type_ = password_type;
}

}  // namespace safe_browsing
