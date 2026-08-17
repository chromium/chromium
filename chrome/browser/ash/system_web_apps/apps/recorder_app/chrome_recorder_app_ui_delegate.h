// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_RECORDER_APP_CHROME_RECORDER_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_RECORDER_APP_CHROME_RECORDER_APP_UI_DELEGATE_H_

#include "ash/webui/recorder_app_ui/recorder_app_ui_delegate.h"
#include "base/memory/raw_ref.h"
#include "components/account_id/account_id.h"
#include "components/soda/constants.h"
#include "content/public/browser/web_ui.h"

class ApplicationLocaleStorage;
class PrefService;

namespace consent_auditor {
class ConsentAuditor;
}  // namespace consent_auditor

namespace signin {
class IdentityManager;
}  // namespace signin

namespace user_manager {
class UserManager;
}  // namespace user_manager

namespace variations {
class VariationsService;
}  // namespace variations

/**
 * Implementation of the RecorderAppUIDelegate interface. Provides the recorder
 * app code in ash/ with functions that only exist in chrome/.
 */
class ChromeRecorderAppUIDelegate : public ash::RecorderAppUIDelegate {
 public:
  // `local_state`, `application_locale_storage`, `variations_service`,
  // `user_manager`, `identity_manager`, and `consent_auditor` must not be null
  // and must outlive `this`.
  ChromeRecorderAppUIDelegate(
      PrefService* local_state,
      const ApplicationLocaleStorage* application_locale_storage,
      variations::VariationsService* variations_service,
      user_manager::UserManager* user_manager,
      const AccountId& account_id,
      signin::IdentityManager* identity_manager,
      consent_auditor::ConsentAuditor* consent_auditor);

  ChromeRecorderAppUIDelegate(const ChromeRecorderAppUIDelegate&) = delete;
  ChromeRecorderAppUIDelegate& operator=(const ChromeRecorderAppUIDelegate&) =
      delete;

  // ash::RecorderAppUIDelegate
  void InstallSoda(speech::LanguageCode language_code) override;

  std::u16string GetLanguageDisplayName(
      speech::LanguageCode language_code) override;

  std::string GetDefaultTranscriptionLanguage() override;

  void OpenAiFeedbackDialog(const std::string& description_template) override;

  bool CanUseGenerativeAiForCurrentProfile() override;

  bool CanUseSpeakerLabelForCurrentProfile() override;

  void RecordSpeakerLabelConsent(
      const sync_pb::UserConsentTypes::RecorderSpeakerLabelConsent& consent)
      override;

  media_device_salt::MediaDeviceSaltService* GetMediaDeviceSaltService(
      content::BrowserContext* context) override;

 private:
  const raw_ref<PrefService> local_state_;
  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
  const raw_ref<variations::VariationsService> variations_service_;
  const raw_ref<user_manager::UserManager> user_manager_;

  const AccountId account_id_;
  const raw_ref<signin::IdentityManager> identity_manager_;
  const raw_ref<consent_auditor::ConsentAuditor> consent_auditor_;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_RECORDER_APP_CHROME_RECORDER_APP_UI_DELEGATE_H_
