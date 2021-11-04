// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/shimless_rma.h"

#include <string>
#include <utility>

#include "ash/grit/ash_shimless_rma_resources.h"
#include "ash/grit/ash_shimless_rma_resources_map.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/webui/shimless_rma/url_constants.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/chromeos/strings/network_element_localized_strings_provider.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_resources.h"

namespace ash {

namespace {

// TODO(crbug/1051793): Replace with webui::SetUpWebUIDataSource() once it no
// longer requires a dependency on //chrome/browser.
void SetUpWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  source->AddResourcePaths(resources);
  source->SetDefaultResource(default_resource);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
}

void AddShimlessRmaStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"currentVersionText", IDS_SHIMLESS_RMA_CURRENT_VERSION},
      {"currentVersionOutOfDateText",
       IDS_SHIMLESS_RMA_CURRENT_VERSION_OUT_OF_DATE},
      {"currentVersionUpToDateText",
       IDS_SHIMLESS_RMA_CURRENT_VERSION_UP_TO_DATE},
      {"repairCompletedTitleText", IDS_SHIMLESS_RMA_REPAIR_COMPLETED},
      {"updateVersionRestartLabel",
       IDS_SHIMLESS_RMA_UPDATE_VERSION_AND_RESTART},
  };

  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->UseStringsJs();
}

}  // namespace

ShimlessRMADialogUI::ShimlessRMADialogUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui),
      shimless_rma_manager_(
          std::make_unique<shimless_rma::ShimlessRmaService>()) {
  auto html_source = base::WrapUnique(
      content::WebUIDataSource::Create(kChromeUIShimlessRMAHost));
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test 'self';");
  html_source->DisableTrustedTypesCSP();

  const auto resources =
      base::make_span(kAshShimlessRmaResources, kAshShimlessRmaResourcesSize);
  SetUpWebUIDataSource(html_source.get(), resources,
                       IDR_ASH_SHIMLESS_RMA_INDEX_HTML);

  AddShimlessRmaStrings(html_source.get());

  ui::network_element::AddLocalizedStrings(html_source.get());
  ui::network_element::AddOncLocalizedStrings(html_source.get());
  ui::network_element::AddDetailsLocalizedStrings(html_source.get());
  ui::network_element::AddConfigLocalizedStrings(html_source.get());
  ui::network_element::AddErrorLocalizedStrings(html_source.get());
  html_source.get()->UseStringsJs();

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source.release());
}

ShimlessRMADialogUI::~ShimlessRMADialogUI() = default;

void ShimlessRMADialogUI::BindInterface(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  ash::GetNetworkConfigService(std::move(receiver));
}

void ShimlessRMADialogUI::BindInterface(
    mojo::PendingReceiver<shimless_rma::mojom::ShimlessRmaService> receiver) {
  DCHECK(shimless_rma_manager_);
  shimless_rma_manager_->BindInterface(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ShimlessRMADialogUI)

}  // namespace ash
