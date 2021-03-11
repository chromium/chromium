// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/update_password_infobar_delegate_android.h"

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/android/password_infobar_utils.h"
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
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  // is_smartlock_branding_enabled indicates whether the user is syncing
  // passwords to their Google Account.
  const bool is_smartlock_branding_enabled =
      password_bubble_experiment::IsSmartLockUser(
          ProfileSyncServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext())));
  InfoBarService::FromWebContents(web_contents)
      ->AddInfoBar(std::make_unique<UpdatePasswordInfoBar>(
          base::WrapUnique(new UpdatePasswordInfoBarDelegate(
              web_contents, std::move(form_to_save),
              is_smartlock_branding_enabled)),
          password_manager::GetAccountInfoForPasswordInfobars(
              profile, /*is_syncing=*/is_smartlock_branding_enabled)));
}

UpdatePasswordInfoBarDelegate::~UpdatePasswordInfoBarDelegate() {
  password_manager::metrics_util::LogUpdateUIDismissalReason(infobar_response_);
  if (auto* recorder = passwords_state_.form_manager()->GetMetricsRecorder()) {
    recorder->RecordUIDismissalReason(infobar_response_);
  }
}

std::u16string UpdatePasswordInfoBarDelegate::GetBranding() const {
  return l10n_util::GetStringUTF16(is_smartlock_branding_enabled_
                                       ? IDS_PASSWORD_MANAGER_SMART_LOCK
                                       : IDS_PASSWORD_MANAGER_TITLE_BRAND);
}

bool UpdatePasswordInfoBarDelegate::ShowMultipleAccounts() const {
  return GetCurrentForms().size() > 1;
}

const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
UpdatePasswordInfoBarDelegate::GetCurrentForms() const {
  return passwords_state_.GetCurrentForms();
}

const std::u16string& UpdatePasswordInfoBarDelegate::GetDefaultUsername()
    const {
  return passwords_state_.form_manager()
      ->GetPendingCredentials()
      .username_value;
}

unsigned int UpdatePasswordInfoBarDelegate::GetDisplayUsernames(
    std::vector<std::u16string>* usernames) {
  return UpdatePasswordInfoBarDelegate::GetDisplayUsernames(
      GetCurrentForms(), GetDefaultUsername(), usernames);
}

// static
unsigned int UpdatePasswordInfoBarDelegate::GetDisplayUsernames(
    const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
        current_forms,
    const std::u16string& default_username,
    std::vector<std::u16string>* usernames) {
  unsigned int selected_username = 0;
  // TODO(crbug.com/1054410): Fix the update logic to use all best matches,
  // rather than current_forms which is best_matches without PSL-matched
  // credentials.
  if (current_forms.size() > 1) {
    // If multiple credentials can be updated, we display a dropdown with all
    // the corresponding usernames.
    for (const auto& form : current_forms) {
      usernames->push_back(GetDisplayUsername(*form));
      if (form->username_value == default_username) {
        selected_username = usernames->size() - 1;
      }
    }
  } else if (default_username.empty()) {
    usernames->push_back(
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN));
  } else {
    usernames->push_back(default_username);
  }
  return selected_username;
}

UpdatePasswordInfoBarDelegate::UpdatePasswordInfoBarDelegate(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_update,
    bool is_smartlock_branding_enabled)
    : infobar_response_(password_manager::metrics_util::NO_DIRECT_INTERACTION),
      is_smartlock_branding_enabled_(is_smartlock_branding_enabled) {
  SetMessage(GetSavePasswordDialogTitleText(
      web_contents->GetVisibleURL(),
      url::Origin::Create(form_to_update->GetURL()),
      PasswordTitleType::UPDATE_PASSWORD));
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

std::u16string UpdatePasswordInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UPDATE_BUTTON);
}

void UpdatePasswordInfoBarDelegate::InfoBarDismissed() {
  infobar_response_ = password_manager::metrics_util::CLICKED_CANCEL;
}

bool UpdatePasswordInfoBarDelegate::Accept() {
  infobar_response_ = password_manager::metrics_util::CLICKED_ACCEPT;
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
