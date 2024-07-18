// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/recorder_app/chrome_recorder_app_ui_delegate.h"

#include "ash/webui/recorder_app_ui/recorder_app_ui_delegate.h"
#include "ash/webui/recorder_app_ui/url_constants.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "components/soda/soda_installer.h"
#include "components/soda/soda_util.h"
#include "url/gurl.h"

ChromeRecorderAppUIDelegate::ChromeRecorderAppUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

void ChromeRecorderAppUIDelegate::InstallSoda(
    speech::LanguageCode language_code) {
  CHECK(speech::IsOnDeviceSpeechRecognitionSupported());
  raw_ptr<PrefService> profile_prefs =
      ProfileManager::GetPrimaryUserProfile()->GetPrefs();
  raw_ptr<PrefService> global_prefs = g_browser_process->local_state();

  auto* soda_installer = speech::SodaInstaller::GetInstance();
  soda_installer->Init(profile_prefs, global_prefs);

  if (soda_installer->IsSodaDownloading(language_code)) {
    return;
  }
  soda_installer->InstallLanguage(speech::GetLanguageName(language_code),
                                  g_browser_process->local_state());
}

void ChromeRecorderAppUIDelegate::OpenAiFeedbackDialog(
    const std::string& description_template) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  chrome::ShowFeedbackPage(/*page_url=*/GURL(ash::kChromeUIRecorderAppURL),
                           /*profile=*/profile,
                           /*source=*/feedback::kFeedbackSourceAI,
                           /*description_template=*/description_template,
                           /*description_placeholder_text=*/std::string(),
                           /*category_tag=*/"chromeos-recorder-app",
                           /*extra_diagnostics=*/std::string());
}

media_device_salt::MediaDeviceSaltService*
ChromeRecorderAppUIDelegate::GetMediaDeviceSaltService(
    content::BrowserContext* context) {
  return MediaDeviceSaltServiceFactory::GetInstance()->GetForBrowserContext(
      context);
}
