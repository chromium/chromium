// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/annotator/untrusted_annotator_ui_config.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/annotator/untrusted_annotator_ui.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/projector/projector_utils.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

UntrustedAnnotatorUIConfig::UntrustedAnnotatorUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  ash::kChromeUIProjectorAnnotatorHost) {}

UntrustedAnnotatorUIConfig::~UntrustedAnnotatorUIConfig() =
    default;

bool UntrustedAnnotatorUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  // TODO(b/342104047): Remove this check once the annotator is completely
  // independant.
  return IsProjectorAppEnabled(profile);
}

std::unique_ptr<content::WebUIController>
UntrustedAnnotatorUIConfig::CreateWebUIController(
    content::WebUI* web_ui,
    const GURL& url) {
  return std::make_unique<ash::UntrustedAnnotatorUI>(web_ui);
}
