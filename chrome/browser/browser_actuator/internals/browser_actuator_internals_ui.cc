// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/internals/browser_actuator_internals_ui.h"

#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/browser_actuator/internals/browser_actuator_internals_ui_mojo_impl.h"
#include "chrome/grit/browser_actuator_internals_resources.h"
#include "chrome/grit/browser_actuator_internals_resources_map.h"
#include "components/browser_actuator/public/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace browser_actuator {

BrowserActuatorInternalsUIConfig::BrowserActuatorInternalsUIConfig()
    : DefaultInternalWebUIConfig(kChromeUIBrowserActuatorInternalsHost) {}

BrowserActuatorInternalsUIConfig::~BrowserActuatorInternalsUIConfig() = default;

bool BrowserActuatorInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(browser_actuator::kBrowserActuator) &&
         base::FeatureList::IsEnabled(
             browser_actuator::kBrowserActuatorInternals);
}

BrowserActuatorInternalsUI::BrowserActuatorInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/false) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIBrowserActuatorInternalsHost);
  webui::SetupWebUIDataSource(
      source, kBrowserActuatorInternalsResources,
      IDR_BROWSER_ACTUATOR_INTERNALS_BROWSER_ACTUATOR_INTERNALS_HTML);
}

BrowserActuatorInternalsUI::~BrowserActuatorInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(BrowserActuatorInternalsUI)

void BrowserActuatorInternalsUI::BindInterface(
    mojo::PendingReceiver<
        browser_actuator_internals::mojom::BrowserActuatorInternalsUIFactory>
        receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void BrowserActuatorInternalsUI::CreateUI(
    mojo::PendingRemote<
        browser_actuator_internals::mojom::BrowserActuatorInternalsPage> page,
    mojo::PendingReceiver<
        browser_actuator_internals::mojom::BrowserActuatorInternalsUI> ui) {
  mojo_impl_ = std::make_unique<BrowserActuatorInternalsUIMojoImpl>(
      std::move(ui), std::move(page));
}

}  // namespace browser_actuator
