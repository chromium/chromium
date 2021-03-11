// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/password_reuse_controller_android.h"

#include "base/callback.h"
#include "chrome/browser/ui/android/safe_browsing/password_reuse_dialog_view_android.h"
#include "components/safe_browsing/core/password_protection/metrics_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace safe_browsing {

PasswordReuseControllerAndroid::PasswordReuseControllerAndroid(
    content::WebContents* web_contents,
    ChromePasswordProtectionService* service,
    ReusedPasswordAccountType password_type,
    OnWarningDone done_callback)
    : service_(service),
      url_(web_contents->GetLastCommittedURL()),
      password_type_(password_type),
      window_android_(web_contents->GetTopLevelNativeWindow()),
      done_callback_(std::move(done_callback)) {
  modal_construction_start_time_ = base::TimeTicks::Now();
  service_->AddObserver(this);
}

PasswordReuseControllerAndroid::~PasswordReuseControllerAndroid() {
  service_->RemoveObserver(this);
  dialog_view_.reset();
  LogModalWarningDialogLifetime(modal_construction_start_time_);
}

void PasswordReuseControllerAndroid::ShowDialog() {
  dialog_view_.reset(new PasswordReuseDialogViewAndroid(this));
  DCHECK(window_android_);
  dialog_view_->Show(window_android_);
}

void PasswordReuseControllerAndroid::CloseDialog() {
  if (done_callback_)
    std::move(done_callback_).Run(WarningAction::CLOSE);
  delete this;
}

std::u16string PasswordReuseControllerAndroid::GetButtonText() const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

std::u16string PasswordReuseControllerAndroid::GetWarningDetailText(
    std::vector<size_t>* placeholder_offsets) const {
  return service_->GetWarningDetailText(password_type_, placeholder_offsets);
}

std::u16string PasswordReuseControllerAndroid::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_SUMMARY);
}

const std::vector<std::u16string>
PasswordReuseControllerAndroid::GetPlaceholdersForSavedPasswordWarningText()
    const {
  return service_->GetPlaceholdersForSavedPasswordWarningText();
}

void PasswordReuseControllerAndroid::OnGaiaPasswordChanged() {
  delete this;
  // Chrome on Android should not be able to capture Gaia password change
  // events.
  NOTREACHED();
}

void PasswordReuseControllerAndroid::OnMarkingSiteAsLegitimate(
    const GURL& url) {
  if (url_.GetWithEmptyPath() == url.GetWithEmptyPath())
    delete this;
  // Modal dialog on Android is above the screen, this function can't be called.
  NOTREACHED();
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

}  // namespace safe_browsing
