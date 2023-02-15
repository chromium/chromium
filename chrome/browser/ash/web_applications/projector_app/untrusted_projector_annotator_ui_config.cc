// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/projector_app/untrusted_projector_annotator_ui_config.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/projector/projector_utils.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/webui_url_constants.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_ui_data_source.h"

ChromeUntrustedProjectorAnnotatorUIDelegate::
    ChromeUntrustedProjectorAnnotatorUIDelegate() = default;

void ChromeUntrustedProjectorAnnotatorUIDelegate::PopulateLoadTimeData(
    content::WebUIDataSource* source) {
  version_info::Channel channel = chrome::GetChannel();
  source->AddBoolean("isDevChannel", channel == version_info::Channel::DEV);
}

UntrustedProjectorAnnotatorUIConfig::UntrustedProjectorAnnotatorUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  ash::kChromeUIProjectorAnnotatorHost) {}

UntrustedProjectorAnnotatorUIConfig::~UntrustedProjectorAnnotatorUIConfig() =
    default;

bool UntrustedProjectorAnnotatorUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return IsProjectorAppEnabled(profile);
}

std::unique_ptr<content::WebUIController>
UntrustedProjectorAnnotatorUIConfig::CreateWebUIController(
    content::WebUI* web_ui) {
  ChromeUntrustedProjectorAnnotatorUIDelegate delegate;
  return std::make_unique<ash::UntrustedProjectorAnnotatorUI>(web_ui,
                                                              &delegate);
}
