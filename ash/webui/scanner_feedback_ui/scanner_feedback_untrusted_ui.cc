// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/scanner_feedback_ui/scanner_feedback_untrusted_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_scanner_feedback_ui_resources.h"
#include "ash/webui/grit/ash_scanner_feedback_ui_resources_map.h"
#include "ash/webui/scanner_feedback_ui/mojom/scanner_feedback_ui.mojom.h"
#include "ash/webui/scanner_feedback_ui/scanner_feedback_page_handler.h"
#include "ash/webui/scanner_feedback_ui/url_constants.h"
#include "base/check_deref.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom-shared.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

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
    : ui::WebDialogUI(web_ui),
      page_handler_(
          CHECK_DEREF(CHECK_DEREF(CHECK_DEREF(web_ui).GetWebContents())
                          .GetBrowserContext())) {
  // Emulate `ui::UntrustedWebUIController`. This should never enable bindings.
  web_ui->SetBindings(content::BindingsPolicySet());

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
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src-elem 'self' theme;");
}

ScannerFeedbackUntrustedUI::~ScannerFeedbackUntrustedUI() = default;

void ScannerFeedbackUntrustedUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void ScannerFeedbackUntrustedUI::BindInterface(
    mojo::PendingReceiver<mojom::scanner_feedback_ui::PageHandler> receiver) {
  page_handler_.Bind(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ScannerFeedbackUntrustedUI)

}  // namespace ash
