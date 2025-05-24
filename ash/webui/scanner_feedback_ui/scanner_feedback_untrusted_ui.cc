// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/scanner_feedback_ui/scanner_feedback_untrusted_ui.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/scanner/scanner_feedback_info.h"
#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_scanner_feedback_ui_resources.h"
#include "ash/webui/grit/ash_scanner_feedback_ui_resources_map.h"
#include "ash/webui/scanner_feedback_ui/mojom/scanner_feedback_ui.mojom.h"
#include "ash/webui/scanner_feedback_ui/scanner_feedback_browser_context_data.h"
#include "ash/webui/scanner_feedback_ui/scanner_feedback_page_handler.h"
#include "ash/webui/scanner_feedback_ui/url_constants.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/browser_context.h"
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

namespace {

constexpr bool HasOverlap(std::string_view prefix, std::string_view suffix) {
  for (size_t i = 0; i < suffix.size(); ++i) {
    std::string_view truncated_suffix = suffix;
    truncated_suffix.remove_suffix(i);
    if (prefix.ends_with(truncated_suffix)) {
      return true;
    }
  }
  return false;
}

// `kScannerFeedbackScreenshotPrefix` and `kScannerFeedbackScreenshotSuffix`
// must not have any "overlap", or else
//     (path.starts_with(kScannerFeedbackScreenshotPrefix) &&
//      path.ends_with(kScannerFeedbackScreenshotSuffix))
// may not necessarily imply that
//     (path.size() >=
//      kScannerFeedbackScreenshotSuffix.size() +
//          kScannerFeedbackScreenshotSuffix.size())
static_assert(!HasOverlap(kScannerFeedbackScreenshotPrefix,
                          kScannerFeedbackScreenshotSuffix));

// Returns the `base::UnguessableToken` ID for a given screenshot URL path.
std::optional<base::UnguessableToken> GetScreenshotId(std::string_view path) {
  if (!path.starts_with(kScannerFeedbackScreenshotPrefix)) {
    return std::nullopt;
  }

  if (!path.ends_with(kScannerFeedbackScreenshotSuffix)) {
    return std::nullopt;
  }

  path.remove_prefix(kScannerFeedbackScreenshotPrefix.size());
  path.remove_suffix(kScannerFeedbackScreenshotSuffix.size());

  return base::UnguessableToken::DeserializeFromString(path);
}

// Returns whether we should handle a given request, given the path.
bool ShouldHandleRequest(base::WeakPtr<content::BrowserContext> browser_context,
                         const std::string& path) {
  std::optional<base::UnguessableToken> id = GetScreenshotId(path);
  if (!id.has_value()) {
    return false;
  }

  CHECK(browser_context);
  ScannerFeedbackInfo* feedback_info =
      GetScannerFeedbackInfoForBrowserContext(*browser_context, *id);

  if (feedback_info == nullptr) {
    return false;
  }

  // Do not handle requests for feedback info without a screenshot.
  return feedback_info->screenshot != nullptr;
}

// Handles the given request and returns the screenshot to the
// `GotDataCallback`.
void HandleRequest(base::WeakPtr<content::BrowserContext> browser_context,
                   const std::string& path,
                   content::WebUIDataSource::GotDataCallback callback) {
  std::optional<base::UnguessableToken> id = GetScreenshotId(path);
  // `GetScreenshotId` is deterministic, so this should always be true as we
  // checked it in `ShouldHandleRequest`.
  CHECK(id.has_value());

  CHECK(browser_context);
  ScannerFeedbackInfo* feedback_info =
      GetScannerFeedbackInfoForBrowserContext(*browser_context, *id);

  // `GetScannerFeedbackInfoForBrowserContext` is _not_ deterministic, but is
  // run synchronously after `ShouldHandleRequest` in
  // `WebUIDataSourceImpl::StartDataRequest`.
  // If this ever changes - for example, it is `PostTask`ed instead, this
  // `CHECK` could fail if the `WebUI` is destroyed between
  // `ShouldHandleRequest` and `HandleRequest`.
  CHECK(feedback_info);

  std::move(callback).Run(feedback_info->screenshot);
}

}  // namespace

ScannerFeedbackUntrustedUIConfig::ScannerFeedbackUntrustedUIConfig()
    : ChromeOSWebUIConfig(content::kChromeUIUntrustedScheme,
                          kScannerFeedbackUntrustedHost) {}

ScannerFeedbackUntrustedUIConfig::~ScannerFeedbackUntrustedUIConfig() = default;

bool ScannerFeedbackUntrustedUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return features::IsScannerEnabled() || features::IsCoralFeatureEnabled();
}

ScannerFeedbackUntrustedUI::ScannerFeedbackUntrustedUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui),
      page_handler_(
          CHECK_DEREF(CHECK_DEREF(CHECK_DEREF(web_ui).GetWebContents())
                          .GetBrowserContext())) {
  // Emulate `ui::UntrustedWebUIController`. This should never enable bindings.
  web_ui->SetBindings(content::BindingsPolicySet());

  // This should be non-null as it was checked above.
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();

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
          browser_context, std::string(kScannerFeedbackUntrustedUrl));

  untrusted_source->AddResourcePaths(kAshScannerFeedbackUiResources);
  // We intentionally do not use `SetDefaultResource` here as we do not want to
  // serve index.html for non-HTML paths.
  untrusted_source->AddResourcePath("", IDR_ASH_SCANNER_FEEDBACK_UI_INDEX_HTML);

  ash::EnableTrustedTypesCSP(untrusted_source);
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src-elem 'self' theme;");
  untrusted_source->AddLocalizedStrings(
      {{"linkAriaLabel", IDS_MAKO_FEEDBACK_LINK_ARIA_LABEL},
       {"title", IDS_MAKO_FEEDBACK_TITLE},
       {"subtitle", IDS_MAKO_FEEDBACK_SUBTITLE},
       {"question", IDS_MAKO_FEEDBACK_QUESTION},
       {"questionPlaceholder", IDS_MAKO_FEEDBACK_QUESTION_PLACEHOLDER},
       {"offensiveOrUnsafe", IDS_MAKO_FEEDBACK_OFFENSIVE_OR_UNSAFE},
       {"factuallyIncorrect", IDS_MAKO_FEEDBACK_FACTUALLY_INCORRECT},
       {"legalIssue", IDS_MAKO_FEEDBACK_LEGAL_ISSUE},
       {"feedbackDisclaimer", IDS_MAKO_FEEDBACK_FEEDBACK_DISCLAIMER},
       {"privacyPolicy", IDS_MAKO_FEEDBACK_PRIVACY_POLICY},
       {"termsOfService", IDS_MAKO_FEEDBACK_TERMS_OF_SERVICE},
       {"cancel", IDS_MAKO_FEEDBACK_CANCEL},
       {"send", IDS_MAKO_FEEDBACK_SEND}});
  untrusted_source->UseStringsJs();

  untrusted_source->SetRequestFilter(
      base::BindRepeating(&ShouldHandleRequest, browser_context->GetWeakPtr()),
      base::BindRepeating(&HandleRequest, browser_context->GetWeakPtr()));
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
