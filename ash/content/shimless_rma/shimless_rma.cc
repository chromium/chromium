// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/content/shimless_rma/shimless_rma.h"

#include <string>

#include "ash/content/shimless_rma/url_constants.h"
#include "ash/grit/ash_shimless_rma_resources.h"
#include "ash/grit/ash_shimless_rma_resources_map.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_resources.h"

namespace ash {

namespace {

// TODO(joonbug): Replace with webui::SetUpWebUIDataSource() once it no
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

}  // namespace

ShimlessRMADialogUI::ShimlessRMADialogUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
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

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source.release());
}

ShimlessRMADialogUI::~ShimlessRMADialogUI() = default;

}  // namespace ash
