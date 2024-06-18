// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/recorder_app/chrome_recorder_app_ui_delegate.h"

#include "ash/webui/recorder_app_ui/recorder_app_ui_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/soda/soda_installer.h"
#include "components/soda/soda_util.h"

ChromeRecorderAppUIDelegate::ChromeRecorderAppUIDelegate(
    content::WebUI* webui) {}

void ChromeRecorderAppUIDelegate::InstallSoda(
    speech::LanguageCode languageCode) {
  CHECK(speech::IsOnDeviceSpeechRecognitionSupported());
  // TODO(pihsun): Have a separate function to initialize / install SODA since
  // this should require user to click download.
  raw_ptr<PrefService> profile_prefs =
      ProfileManager::GetPrimaryUserProfile()->GetPrefs();
  raw_ptr<PrefService> global_prefs = g_browser_process->local_state();

  auto* soda_installer = speech::SodaInstaller::GetInstance();
  soda_installer->Init(profile_prefs, global_prefs);

  if (soda_installer->IsSodaDownloading(languageCode)) {
    return;
  }
  soda_installer->InstallLanguage(speech::GetLanguageName(languageCode),
                                  g_browser_process->local_state());
}
