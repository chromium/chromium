// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/internals/browser_actuator_internals_ui.h"

#include "base/feature_list.h"
#include "chrome/grit/browser_actuator_internals_resources.h"
#include "chrome/grit/browser_actuator_internals_resources_map.h"
#include "components/browser_actuator/public/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/webui_util.h"

BrowserActuatorInternalsUIConfig::BrowserActuatorInternalsUIConfig()
    : DefaultInternalWebUIConfig(kChromeUIBrowserActuatorInternalsHost) {}

BrowserActuatorInternalsUIConfig::~BrowserActuatorInternalsUIConfig() = default;

bool BrowserActuatorInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(browser_actuator::kBrowserActuator);
}

BrowserActuatorInternalsUI::BrowserActuatorInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIBrowserActuatorInternalsHost);
  webui::SetupWebUIDataSource(
      source, kBrowserActuatorInternalsResources,
      IDR_BROWSER_ACTUATOR_INTERNALS_BROWSER_ACTUATOR_INTERNALS_HTML);
}

BrowserActuatorInternalsUI::~BrowserActuatorInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(BrowserActuatorInternalsUI)
