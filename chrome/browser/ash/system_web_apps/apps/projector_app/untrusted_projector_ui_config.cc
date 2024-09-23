// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/projector_app/untrusted_projector_ui_config.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/projector/projector_utils.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/webui_url_constants.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/blink/public/common/features.h"

ChromeUntrustedProjectorUIDelegate::ChromeUntrustedProjectorUIDelegate() =
    default;

void ChromeUntrustedProjectorUIDelegate::PopulateLoadTimeData(
    content::WebUIDataSource* source) {
  version_info::Channel channel = chrome::GetChannel();
  source->AddBoolean("isDevChannel", channel == version_info::Channel::DEV);
  source->AddBoolean("isDebugMode", ash::features::IsProjectorAppDebugMode());
  source->AddBoolean("isCustomThumbnailEnabled",
                     ash::features::IsProjectorCustomThumbnailEnabled());
  // The local playback feature depends on the file handling API.
  source->AddBoolean(
      "isLocalPlaybackEnabled",
      base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI));
  source->AddBoolean("isMutingEnabled",
                     ash::features::IsProjectorMutingEnabled());
  source->AddBoolean("isPwaRedirectEnabled",
                     ash::features::IsProjectorRedirectToPwaEnabled());
  source->AddBoolean("isTranscriptChapterTitleEnabled",
                     ash::features::IsProjectorTranscriptChapterTitleEnabled());
  source->AddBoolean("isDynamicColorsEnabled",
                     ash::features::IsProjectorDynamicColorsEnabled());
  source->AddBoolean("isGm3Enabled", ash::features::IsProjectorGm3Enabled());

  source->AddBoolean(
      "isInternalServerSideSpeechRecognitionEnabled",
      ash::features::IsInternalServerSideSpeechRecognitionEnabled());
  source->AddString("appLocale", g_browser_process->GetApplicationLocale());
}

UntrustedProjectorUIConfig::UntrustedProjectorUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  ash::kChromeUIProjectorAppHost) {}

UntrustedProjectorUIConfig::~UntrustedProjectorUIConfig() = default;

bool UntrustedProjectorUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return IsProjectorAppEnabled(profile);
}

std::unique_ptr<content::WebUIController>
UntrustedProjectorUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                                  const GURL& url) {
  ChromeUntrustedProjectorUIDelegate delegate;
  return std::make_unique<ash::UntrustedProjectorUI>(
      web_ui, &delegate, Profile::FromWebUI(web_ui)->GetPrefs());
}
