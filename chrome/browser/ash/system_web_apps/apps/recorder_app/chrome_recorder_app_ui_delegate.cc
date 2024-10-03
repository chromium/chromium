// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/recorder_app/chrome_recorder_app_ui_delegate.h"

#include "ash/webui/recorder_app_ui/recorder_app_ui_delegate.h"
#include "ash/webui/recorder_app_ui/url_constants.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/feedback/feedback_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/soda/soda_installer.h"
#include "components/soda/soda_util.h"
#include "url/gurl.h"

ChromeRecorderAppUIDelegate::ChromeRecorderAppUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

void ChromeRecorderAppUIDelegate::InstallSoda(
    speech::LanguageCode language_code) {
  CHECK(speech::IsOnDeviceSpeechRecognitionSupported());
  raw_ptr<PrefService> profile_prefs = Profile::FromWebUI(web_ui_)->GetPrefs();
  raw_ptr<PrefService> global_prefs = g_browser_process->local_state();

  auto* soda_installer = speech::SodaInstaller::GetInstance();
  // InstallSoda and InstallLanguage calls DLC download, which will ignore
  // duplicate request, so this is safe without checking if an ongoing install
  // is in progress.
  // TODO: b/369730074 - Ideally we should also remember whether user enabled
  // transcription in a user pref, and ask SODA to preload on ash launch (in
  // `IsAnyFeatureUsingSodaEnabled`) if it's enabled so the app can get
  // transcription faster.
  soda_installer->InstallSoda(global_prefs);
  soda_installer->InstallLanguage(speech::GetLanguageName(language_code),
                                  g_browser_process->local_state());
}

void ChromeRecorderAppUIDelegate::OpenAiFeedbackDialog(
    const std::string& description_template) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  base::Value::Dict ai_metadata;
  ai_metadata.Set(feedback::kConchMetadataKey, "true");
  chrome::ShowFeedbackPage(/*page_url=*/GURL(ash::kChromeUIRecorderAppURL),
                           /*profile=*/profile,
                           /*source=*/feedback::kFeedbackSourceAI,
                           /*description_template=*/description_template,
                           /*description_placeholder_text=*/std::string(),
                           /*category_tag=*/"chromeos-recorder-app",
                           /*extra_diagnostics=*/std::string(),
                           /*autofill_metadata=*/base::Value::Dict(),
                           /*ai_metadata=*/std::move(ai_metadata));
}

media_device_salt::MediaDeviceSaltService*
ChromeRecorderAppUIDelegate::GetMediaDeviceSaltService(
    content::BrowserContext* context) {
  return MediaDeviceSaltServiceFactory::GetInstance()->GetForBrowserContext(
      context);
}

bool ChromeRecorderAppUIDelegate::CanUseGenerativeAiForCurrentProfile() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (identity_manager == nullptr) {
    return false;
  }

  const auto account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (account_id.empty()) {
    return false;
  }

  const AccountInfo extended_account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(account_id);
  return extended_account_info.capabilities
             .can_use_generative_ai_in_recorder_app() == signin::Tribool::kTrue;
}

bool ChromeRecorderAppUIDelegate::CanUseSpeakerLabelForCurrentProfile() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (identity_manager == nullptr) {
    return false;
  }

  const auto account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (account_id.empty()) {
    return false;
  }

  const AccountInfo extended_account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(account_id);
  return extended_account_info.capabilities
             .can_use_speaker_label_in_recorder_app() == signin::Tribool::kTrue;
}

void ChromeRecorderAppUIDelegate::RecordSpeakerLabelConsent(
    const sync_pb::UserConsentTypes::RecorderSpeakerLabelConsent& consent) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  DCHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  const CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  ConsentAuditorFactory::GetForProfile(profile)
      ->RecordRecorderSpeakerLabelConsent(account_id, consent);
}
