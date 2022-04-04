// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/media_app/media_app_guest_ui_config.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

ChromeMediaAppGuestUIDelegate::ChromeMediaAppGuestUIDelegate() = default;

void ChromeMediaAppGuestUIDelegate::PopulateLoadTimeData(
    content::WebUI* web_ui,
    content::WebUIDataSource* source) {
  Profile* profile = Profile::FromWebUI(web_ui);
  PrefService* pref_service = profile->GetPrefs();

  source->AddString("appLocale", g_browser_process->GetApplicationLocale());
  source->AddBoolean(
      "audioHandler",
      base::FeatureList::IsEnabled(chromeos::features::kMediaAppHandlesAudio));
  source->AddBoolean("pdfInInk", base::FeatureList::IsEnabled(
                                     chromeos::features::kMediaAppHandlesPdf));
  source->AddBoolean("pdfReadonly",
                     !pref_service->GetBoolean(prefs::kPdfAnnotationsEnabled));
  source->AddBoolean(
      "pdfTextAnnotation",
      base::FeatureList::IsEnabled(chromeos::features::kMediaAppHandlesPdf));
  source->AddBoolean(
      "newZeroState",
      base::FeatureList::IsEnabled(chromeos::features::kMediaAppHandlesPdf));
  version_info::Channel channel = chrome::GetChannel();
  source->AddBoolean("colorThemes",
                     chromeos::features::IsDarkLightModeEnabled());
  source->AddBoolean("flagsMenu", channel != version_info::Channel::BETA &&
                                      channel != version_info::Channel::STABLE);
  source->AddBoolean("isDevChannel", channel == version_info::Channel::DEV);
}

MediaAppGuestUIConfig::MediaAppGuestUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  ash::kChromeUIMediaAppHost) {}

MediaAppGuestUIConfig::~MediaAppGuestUIConfig() = default;

std::unique_ptr<content::WebUIController>
MediaAppGuestUIConfig::CreateWebUIController(content::WebUI* web_ui) {
  ChromeMediaAppGuestUIDelegate delegate;
  return std::make_unique<ash::MediaAppGuestUI>(web_ui, &delegate);
}
