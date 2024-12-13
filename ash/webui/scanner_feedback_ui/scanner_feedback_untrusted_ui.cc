// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/scanner_feedback_ui/scanner_feedback_untrusted_ui.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_scanner_feedback_ui_resources.h"
#include "ash/webui/grit/ash_scanner_feedback_ui_resources_map.h"
#include "ash/webui/scanner_feedback_ui/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace ash {

ScannerFeedbackUntrustedUIConfig::ScannerFeedbackUntrustedUIConfig()
    : ChromeOSWebUIConfig(content::kChromeUIUntrustedScheme,
                          kScannerFeedbackUntrustedHost) {}

ScannerFeedbackUntrustedUIConfig::~ScannerFeedbackUntrustedUIConfig() = default;

bool ScannerFeedbackUntrustedUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return features::IsScannerEnabled();
}

ScannerFeedbackUntrustedUI::ScannerFeedbackUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  // `content::WebUIDataSource`s are stored on the browser context. If an
  // existing `content::WebUIDataSource` exists in the browser context for the
  // given source name, calling `CreateAndAdd` will destroy the previous one.
  //
  // This destruction is unnecessary, as our `content::WebUIDataSource` is
  // deterministic per browser context. However, there is no way of determining
  // whether a browser context already has a data source for a given source
  // name, as that would require calling
  //
  //     content::URLDataManagerBackend::GetForBrowserContext(browser_context)
  //         ->data_sources()
  //         .contains(source_name)
  //
  // which uses `URLDataManagerBackend` in //content/browser - not available to
  // Ash.
  //
  // Note that `content::URLDataSource::ShouldReplaceExistingSource` has a TODO
  // comment that all callers should be converted to _not_ replace existing data
  // sources, so this may change in the future.
  content::WebUIDataSource* untrusted_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          std::string(kScannerFeedbackUntrustedUrl));

  untrusted_source->AddResourcePaths(kAshScannerFeedbackUiResources);
  // We intentionally do not use `SetDefaultResource` here as we do not want to
  // serve index.html for non-HTML paths.
  untrusted_source->AddResourcePath("", IDR_ASH_SCANNER_FEEDBACK_UI_INDEX_HTML);

  ash::EnableTrustedTypesCSP(untrusted_source);
}

ScannerFeedbackUntrustedUI::~ScannerFeedbackUntrustedUI() = default;

}  // namespace ash
