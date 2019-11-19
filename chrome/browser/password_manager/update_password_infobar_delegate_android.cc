// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/update_password_infobar_delegate_android.h"

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/android/infobars/update_password_infobar.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

// static
void UpdatePasswordInfoBarDelegate::Create(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save) {
  const bool is_smartlock_branding_enabled =
      password_bubble_experiment::IsSmartLockUser(
          ProfileSyncServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext())));
  InfoBarService::FromWebContents(web_contents)
      ->AddInfoBar(std::make_unique<UpdatePasswordInfoBar>(
          base::WrapUnique(new UpdatePasswordInfoBarDelegate(
              web_contents, std::move(form_to_save),
              is_smartlock_branding_enabled))));
}

UpdatePasswordInfoBarDelegate::~UpdatePasswordInfoBarDelegate() {
  password_manager::metrics_util::LogUpdateUIDismissalReason(infobar_response_);
  if (auto* recorder = passwords_state_.form_manager()->GetMetricsRecorder()) {
    recorder->RecordUIDismissalReason(infobar_response_);
  }
}

base::string16 UpdatePasswordInfoBarDelegate::GetBranding() const {
  return l10n_util::GetStringUTF16(is_smartlock_branding_enabled_
                                       ? IDS_PASSWORD_MANAGER_SMART_LOCK
                                       : IDS_PASSWORD_MANAGER_TITLE_BRAND);
}

bool UpdatePasswordInfoBarDelegate::ShowMultipleAccounts() const {
  return GetCurrentForms().size() > 1;
}

const std::vector<std::unique_ptr<autofill::PasswordForm>>&
UpdatePasswordInfoBarDelegate::GetCurrentForms() const {
  return passwords_state_.GetCurrentForms();
}

UpdatePasswordInfoBarDelegate::UpdatePasswordInfoBarDelegate(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_update,
    bool is_smartlock_branding_enabled)
    : infobar_response_(password_manager::metrics_util::NO_DIRECT_INTERACTION),
      is_smartlock_branding_enabled_(is_smartlock_branding_enabled) {
  base::string16 message;
  GetSavePasswordDialogTitleTextAndLinkRange(
      web_contents->GetVisibleURL(), form_to_update->GetOrigin(),
      PasswordTitleType::UPDATE_PASSWORD, &message);
  SetMessage(message);
  if (is_smartlock_branding_enabled)
    SetDetailsMessage(l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD_FOOTER));

  if (auto* recorder = form_to_update->GetMetricsRecorder()) {
    recorder->RecordPasswordBubbleShown(
        form_to_update->GetCredentialSource(),
        password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE);
  }

  passwords_state_.set_client(
      ChromePasswordManagerClient::FromWebContents(web_contents));
  passwords_state_.OnUpdatePassword(std::move(form_to_update));
}

infobars::InfoBarDelegate::InfoBarIdentifier
UpdatePasswordInfoBarDelegate::GetIdentifier() const {
  return UPDATE_PASSWORD_INFOBAR_DELEGATE_MOBILE;
}

int UpdatePasswordInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 UpdatePasswordInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UPDATE_BUTTON);
}

void UpdatePasswordInfoBarDelegate::InfoBarDismissed() {
  infobar_response_ = password_manager::metrics_util::CLICKED_CANCEL;
}

bool UpdatePasswordInfoBarDelegate::Accept() {
  infobar_response_ = password_manager::metrics_util::CLICKED_SAVE;
  UpdatePasswordInfoBar* update_password_infobar =
      static_cast<UpdatePasswordInfoBar*>(infobar());
  password_manager::PasswordFormManagerForUI* form_manager =
      passwords_state_.form_manager();
  if (ShowMultipleAccounts()) {
    int form_index = update_password_infobar->GetIdOfSelectedUsername();
    DCHECK_GE(form_index, 0);
    DCHECK_LT(static_cast<size_t>(form_index), GetCurrentForms().size());
    UpdatePasswordFormUsernameAndPassword(
        GetCurrentForms()[form_index]->username_value,
        form_manager->GetPendingCredentials().password_value, form_manager);
  }
  form_manager->Save();
  return true;
}

bool UpdatePasswordInfoBarDelegate::Cancel() {
  infobar_response_ = password_manager::metrics_util::CLICKED_CANCEL;
  return true;
}
