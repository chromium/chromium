// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/recorder_app/chrome_recorder_app_ui_delegate.h"

#include "ash/constants/generative_ai_country_restrictions.h"
#include "ash/webui/recorder_app_ui/recorder_app_ui_delegate.h"
#include "ash/webui/recorder_app_ui/url_constants.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/feedback/feedback_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "components/soda/soda_util.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/variations/service/variations_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

ChromeRecorderAppUIDelegate::ChromeRecorderAppUIDelegate(
    PrefService* local_state,
    const ApplicationLocaleStorage* application_locale_storage,
    variations::VariationsService* variations_service,
    user_manager::UserManager* user_manager,
    const AccountId& account_id,
    signin::IdentityManager* identity_manager,
    consent_auditor::ConsentAuditor* consent_auditor)
    : local_state_(CHECK_DEREF(local_state)),
      application_locale_storage_(CHECK_DEREF(application_locale_storage)),
      variations_service_(CHECK_DEREF(variations_service)),
      user_manager_(CHECK_DEREF(user_manager)),
      account_id_(account_id),
      identity_manager_(CHECK_DEREF(identity_manager)),
      consent_auditor_(CHECK_DEREF(consent_auditor)) {}

void ChromeRecorderAppUIDelegate::InstallSoda(
    speech::LanguageCode language_code) {
  CHECK(speech::IsOnDeviceSpeechRecognitionSupported());

  auto* soda_installer = speech::SodaInstaller::GetInstance();
  // InstallSoda and InstallLanguage calls DLC download, which will ignore
  // duplicate request, so this is safe without checking if an ongoing install
  // is in progress.
  // TODO: b/369730074 - Ideally we should also remember whether user enabled
  // transcription in a user pref, and ask SODA to preload on ash launch (in
  // `IsAnyFeatureUsingSodaEnabled`) if it's enabled so the app can get
  // transcription faster.
  soda_installer->InstallSoda(&local_state_.get());
  soda_installer->InstallLanguage(speech::GetLanguageName(language_code),
                                  &local_state_.get());
}

std::u16string ChromeRecorderAppUIDelegate::GetLanguageDisplayName(
    speech::LanguageCode language_code) {
  return l10n_util::GetDisplayNameForLocale(
      speech::GetLanguageName(language_code),
      application_locale_storage_->Get(), /*is_for_ui=*/true);
}

std::string ChromeRecorderAppUIDelegate::GetDefaultTranscriptionLanguage() {
  const user_manager::User& user =
      CHECK_DEREF(user_manager_->FindUser(account_id_));
  const PrefService& prefs = CHECK_DEREF(user.GetProfilePrefs());
  return std::string(speech::GetDefaultLiveCaptionLanguage(
      application_locale_storage_->Get(), prefs));
}

void ChromeRecorderAppUIDelegate::OpenAiFeedbackDialog(
    const std::string& description_template) {
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          account_id_));

  base::DictValue ai_metadata;
  ai_metadata.Set(feedback::kConchMetadataKey, "true");
  chrome::ShowFeedbackPage(/*page_url=*/GURL(ash::kChromeUIRecorderAppURL),
                           /*profile=*/profile,
                           /*source=*/feedback::kFeedbackSourceAI,
                           /*description_template=*/description_template,
                           /*description_placeholder_text=*/std::string(),
                           /*category_tag=*/"chromeos-recorder-app",
                           /*extra_diagnostics=*/std::string(),
                           /*autofill_metadata=*/base::DictValue(),
                           /*ai_metadata=*/std::move(ai_metadata));
}

media_device_salt::MediaDeviceSaltService*
ChromeRecorderAppUIDelegate::GetMediaDeviceSaltService(
    content::BrowserContext* context) {
  return MediaDeviceSaltServiceFactory::GetInstance()->GetForBrowserContext(
      context);
}

bool ChromeRecorderAppUIDelegate::CanUseGenerativeAiForCurrentProfile() {
  const auto account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (account_id.empty()) {
    return false;
  }

  const AccountInfo extended_account_info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
  if (extended_account_info.GetAccountCapabilities()
          .can_use_generative_ai_in_recorder_app() != signin::Tribool::kTrue) {
    return false;
  }

  // Check location restrictions.
  return ash::IsGenerativeAiAllowedForCountry(
      variations_service_->GetLatestCountry());
}

bool ChromeRecorderAppUIDelegate::CanUseSpeakerLabelForCurrentProfile() {
  const auto account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (account_id.empty()) {
    return false;
  }

  const AccountInfo extended_account_info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
  return extended_account_info.GetAccountCapabilities()
             .can_use_speaker_label_in_recorder_app() == signin::Tribool::kTrue;
}

void ChromeRecorderAppUIDelegate::RecordSpeakerLabelConsent(
    const sync_pb::UserConsentTypes::RecorderSpeakerLabelConsent& consent) {
  DCHECK(identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  const GaiaId gaia_id =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;

  consent_auditor_->RecordRecorderSpeakerLabelConsent(gaia_id, consent);
}
