// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/projector_app/untrusted_projector_ui_config.h"

#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/webui_url_constants.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_ui_data_source.h"

ChromeUntrustedProjectorUIDelegate::ChromeUntrustedProjectorUIDelegate() =
    default;

void ChromeUntrustedProjectorUIDelegate::PopulateLoadTimeData(
    content::WebUIDataSource* source) {
  version_info::Channel channel = chrome::GetChannel();
  source->AddBoolean("isDevChannel", channel == version_info::Channel::DEV);
}

UntrustedProjectorUIConfig::UntrustedProjectorUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  ash::kChromeUIProjectorAppHost) {}

UntrustedProjectorUIConfig::~UntrustedProjectorUIConfig() = default;

std::unique_ptr<content::WebUIController>
UntrustedProjectorUIConfig::CreateWebUIController(content::WebUI* web_ui) {
  ChromeUntrustedProjectorUIDelegate delegate;
  return std::make_unique<ash::UntrustedProjectorUI>(web_ui, &delegate);
}
