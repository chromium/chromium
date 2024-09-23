// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/page_handler.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
#include "components/subresource_filter/content/browser/devtools_interaction_tracker.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "ui/gfx/image/image.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/printing/browser/print_to_pdf/pdf_print_utils.h"
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINTING)

#if BUILDFLAG(ENABLE_PRINTING)

template <typename T>
std::optional<T> OptionalFromMaybe(const protocol::Maybe<T>& maybe) {
  return maybe.has_value() ? std::optional<T>(maybe.value()) : std::nullopt;
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
using ActivePrintManager = printing::PrintViewManager;
#else
using ActivePrintManager = printing::PrintViewManagerBasic;
#endif

#endif  // BUILDFLAG(ENABLE_PRINTING)

PageHandler::PageHandler(scoped_refptr<content::DevToolsAgentHost> agent_host,
                         content::WebContents* web_contents,
                         protocol::UberDispatcher* dispatcher)
    : agent_host_(agent_host), web_contents_(web_contents->GetWeakPtr()) {
  protocol::Page::Dispatcher::wire(dispatcher, this);
}

PageHandler::~PageHandler() {
  Disable();
}

void PageHandler::ToggleAdBlocking(bool enabled) {
  if (!web_contents_)
    return;

  // Create the DevtoolsInteractionTracker lazily (note that this call is a
  // no-op if the object was already created).
  subresource_filter::DevtoolsInteractionTracker::CreateForWebContents(
      web_contents_.get());

  subresource_filter::DevtoolsInteractionTracker::FromWebContents(
      web_contents_.get())
      ->ToggleForceActivation(enabled);
}

protocol::Response PageHandler::Enable() {
  enabled_ = true;
  // Do not mark the command as handled. Let it fall through instead, so that
  // the handler in content gets a chance to process the command.
  return protocol::Response::FallThrough();
}

protocol::Response PageHandler::Disable() {
  enabled_ = false;
  ToggleAdBlocking(false /* enable */);
  SetSPCTransactionMode(protocol::Page::AutoResponseModeEnum::None);
  // Do not mark the command as handled. Let it fall through instead, so that
  // the handler in content gets a chance to process the command.
  return protocol::Response::FallThrough();
}

protocol::Response PageHandler::SetAdBlockingEnabled(bool enabled) {
  if (!enabled_)
    return protocol::Response::ServerError("Page domain is disabled.");
  ToggleAdBlocking(enabled);
  return protocol::Response::Success();
}

protocol::Response PageHandler::SetSPCTransactionMode(
    const protocol::String& mode) {
  if (!web_contents_)
    return protocol::Response::ServerError("No web contents to host a dialog.");

  payments::SPCTransactionMode spc_mode = payments::SPCTransactionMode::NONE;
  if (mode == protocol::Page::AutoResponseModeEnum::AutoAccept) {
    spc_mode = payments::SPCTransactionMode::AUTOACCEPT;
  } else if (mode == protocol::Page::AutoResponseModeEnum::AutoReject) {
    spc_mode = payments::SPCTransactionMode::AUTOREJECT;
  } else if (mode == protocol::Page::AutoResponseModeEnum::AutoOptOut) {
    spc_mode = payments::SPCTransactionMode::AUTOOPTOUT;
  } else if (mode != protocol::Page::AutoResponseModeEnum::None) {
    return protocol::Response::ServerError("Unrecognized mode value");
  }

  auto* payment_request_manager =
      payments::PaymentRequestWebContentsManager::GetOrCreateForWebContents(
          *web_contents_);
  payment_request_manager->SetSPCTransactionMode(spc_mode);
  return protocol::Response::Success();
}

protocol::Response PageHandler::SetRPHRegistrationMode(
    const protocol::String& mode) {
  if (!web_contents_) {
    return protocol::Response::ServerError("No web contents to host a dialog.");
  }

  custom_handlers::RphRegistrationMode rph_mode =
      custom_handlers::RphRegistrationMode::kNone;
  if (mode == protocol::Page::AutoResponseModeEnum::AutoAccept) {
    rph_mode = custom_handlers::RphRegistrationMode::kAutoAccept;
  } else if (mode == protocol::Page::AutoResponseModeEnum::AutoReject) {
    rph_mode = custom_handlers::RphRegistrationMode::kAutoReject;
  } else if (mode != protocol::Page::AutoResponseModeEnum::None) {
    return protocol::Response::ServerError("Unrecognized mode value");
  }

  custom_handlers::ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          web_contents_->GetBrowserContext());
  registry->SetRphRegistrationMode(rph_mode);
  return protocol::Response::Success();
}

void PageHandler::GetInstallabilityErrors(
    std::unique_ptr<GetInstallabilityErrorsCallback> callback) {
  auto errors = std::make_unique<protocol::Array<std::string>>();
  webapps::InstallableManager* manager =
      web_contents_
          ? webapps::InstallableManager::FromWebContents(web_contents_.get())
          : nullptr;
  if (!manager) {
    callback->sendFailure(
        protocol::Response::ServerError("Unable to fetch errors for target"));
    return;
  }
  manager->GetAllErrors(base::BindOnce(&PageHandler::GotInstallabilityErrors,
                                       std::move(callback)));
}

// static
void PageHandler::GotInstallabilityErrors(
    std::unique_ptr<GetInstallabilityErrorsCallback> callback,
    std::vector<content::InstallabilityError> installability_errors) {
  auto result_installability_errors =
      std::make_unique<protocol::Array<protocol::Page::InstallabilityError>>();
  for (const auto& installability_error : installability_errors) {
    auto installability_error_arguments = std::make_unique<
        protocol::Array<protocol::Page::InstallabilityErrorArgument>>();
    for (const auto& error_argument :
         installability_error.installability_error_arguments) {
      installability_error_arguments->emplace_back(
          protocol::Page::InstallabilityErrorArgument::Create()
              .SetName(error_argument.name)
              .SetValue(error_argument.value)
              .Build());
    }
    result_installability_errors->emplace_back(
        protocol::Page::InstallabilityError::Create()
            .SetErrorId(installability_error.error_id)
            .SetErrorArguments(std::move(installability_error_arguments))
            .Build());
  }
  callback->sendSuccess(std::move(result_installability_errors));
}

void PageHandler::GetManifestIcons(
    std::unique_ptr<GetManifestIconsCallback> callback) {
  webapps::InstallableManager* manager =
      web_contents_
          ? webapps::InstallableManager::FromWebContents(web_contents_.get())
          : nullptr;

  if (!manager) {
    callback->sendFailure(
        protocol::Response::ServerError("Unable to fetch icons for target"));
    return;
  }

  manager->GetPrimaryIcon(
      base::BindOnce(&PageHandler::GotManifestIcons, std::move(callback)));
}

void PageHandler::GotManifestIcons(
    std::unique_ptr<GetManifestIconsCallback> callback,
    const SkBitmap* primary_icon) {
  protocol::Maybe<protocol::Binary> primaryIconAsBinary;

  if (primary_icon && !primary_icon->empty()) {
    primaryIconAsBinary = protocol::Binary::fromRefCounted(
        gfx::Image::CreateFrom1xBitmap(*primary_icon).As1xPNGBytes());
  }

  callback->sendSuccess(std::move(primaryIconAsBinary));
}

void PageHandler::PrintToPDF(protocol::Maybe<bool> landscape,
                             protocol::Maybe<bool> display_header_footer,
                             protocol::Maybe<bool> print_background,
                             protocol::Maybe<double> scale,
                             protocol::Maybe<double> paper_width,
                             protocol::Maybe<double> paper_height,
                             protocol::Maybe<double> margin_top,
                             protocol::Maybe<double> margin_bottom,
                             protocol::Maybe<double> margin_left,
                             protocol::Maybe<double> margin_right,
                             protocol::Maybe<protocol::String> page_ranges,
                             protocol::Maybe<protocol::String> header_template,
                             protocol::Maybe<protocol::String> footer_template,
                             protocol::Maybe<bool> prefer_css_page_size,
                             protocol::Maybe<protocol::String> transfer_mode,
                             protocol::Maybe<bool> generate_tagged_pdf,
                             protocol::Maybe<bool> generate_document_outline,
                             std::unique_ptr<PrintToPDFCallback> callback) {
  DCHECK(callback);

#if BUILDFLAG(ENABLE_PRINTING)
  if (!web_contents_) {
    callback->sendFailure(
        protocol::Response::ServerError("No web contents to print"));
    return;
  }

  absl::variant<printing::mojom::PrintPagesParamsPtr, std::string>
      print_pages_params = print_to_pdf::GetPrintPagesParams(
          web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(),
          OptionalFromMaybe<bool>(landscape),
          OptionalFromMaybe<bool>(display_header_footer),
          OptionalFromMaybe<bool>(print_background),
          OptionalFromMaybe<double>(scale),
          OptionalFromMaybe<double>(paper_width),
          OptionalFromMaybe<double>(paper_height),
          OptionalFromMaybe<double>(margin_top),
          OptionalFromMaybe<double>(margin_bottom),
          OptionalFromMaybe<double>(margin_left),
          OptionalFromMaybe<double>(margin_right),
          OptionalFromMaybe<std::string>(header_template),
          OptionalFromMaybe<std::string>(footer_template),
          OptionalFromMaybe<bool>(prefer_css_page_size),
          OptionalFromMaybe<bool>(generate_tagged_pdf),
          OptionalFromMaybe<bool>(generate_document_outline));
  if (absl::holds_alternative<std::string>(print_pages_params)) {
    callback->sendFailure(protocol::Response::InvalidParams(
        absl::get<std::string>(print_pages_params)));
    return;
  }

  DCHECK(absl::holds_alternative<printing::mojom::PrintPagesParamsPtr>(
      print_pages_params));

  bool return_as_stream =
      transfer_mode.value_or("") ==
      protocol::Page::PrintToPDF::TransferModeEnum::ReturnAsStream;

  // First check if headless printer manager is active and use it if so.
  // Note that headless mode uses alternate print manager that shortcuts
  // most of the regular print manager calls providing only the PrintToPDF
  // functionality.
  if (auto* print_manager = headless::HeadlessPrintManager::FromWebContents(
          web_contents_.get())) {
    print_manager->PrintToPdf(
        web_contents_->GetPrimaryMainFrame(), page_ranges.value_or(""),
        std::move(absl::get<printing::mojom::PrintPagesParamsPtr>(
            print_pages_params)),
        base::BindOnce(&PageHandler::OnPDFCreated,
                       weak_ptr_factory_.GetWeakPtr(), return_as_stream,
                       std::move(callback)));
    return;
  }

  // Try the regular print manager. See printing::InitializePrinting()
  // for details.
  if (auto* print_manager =
          ActivePrintManager::FromWebContents(web_contents_.get())) {
    print_manager->PrintToPdf(
        web_contents_->GetPrimaryMainFrame(), page_ranges.value_or(""),
        std::move(absl::get<printing::mojom::PrintPagesParamsPtr>(
            print_pages_params)),
        base::BindOnce(&PageHandler::OnPDFCreated,
                       weak_ptr_factory_.GetWeakPtr(), return_as_stream,
                       std::move(callback)));
    return;
  }
#endif  // BUILDFLAG(ENABLE_PRINTING)

  callback->sendFailure(
      protocol::Response::ServerError("Printing is not available"));
}

void PageHandler::GetAppId(std::unique_ptr<GetAppIdCallback> callback) {
  webapps::InstallableManager* manager =
      web_contents_
          ? webapps::InstallableManager::FromWebContents(web_contents_.get())
          : nullptr;

  if (!manager) {
    callback->sendFailure(
        protocol::Response::ServerError("Unable to fetch app id for target"));
    return;
  }

  webapps::InstallableParams params;
  manager->GetData(params, base::BindOnce(&PageHandler::OnDidGetManifest,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          std::move(callback)));
}

void PageHandler::OnDidGetManifest(std::unique_ptr<GetAppIdCallback> callback,
                                   const webapps::InstallableData& data) {
  if (data.manifest_url->is_empty()) {
    callback->sendSuccess(protocol::Maybe<protocol::String>(),
                          protocol::Maybe<protocol::String>());
    return;
  }
  // Either both the id and start_url are present, or they are both empty.
  std::string current_app_id_str;
  std::string recommended_manifest_id_path_only;
  if (data.manifest->id.is_valid()) {
    CHECK(data.manifest->start_url.is_valid());
    current_app_id_str = data.manifest->id.spec();
    recommended_manifest_id_path_only =
        web_app::GenerateManifestIdFromStartUrlOnly(data.manifest->start_url)
            .PathForRequest();
  } else {
    CHECK(!data.manifest->start_url.is_valid());
  }
  callback->sendSuccess(current_app_id_str, recommended_manifest_id_path_only);
}

#if BUILDFLAG(ENABLE_PRINTING)
void PageHandler::OnPDFCreated(bool return_as_stream,
                               std::unique_ptr<PrintToPDFCallback> callback,
                               print_to_pdf::PdfPrintResult print_result,
                               scoped_refptr<base::RefCountedMemory> data) {
  if (print_result != print_to_pdf::PdfPrintResult::kPrintSuccess) {
    callback->sendFailure(protocol::Response::ServerError(
        print_to_pdf::PdfPrintResultToString(print_result)));
    return;
  }

  if (return_as_stream) {
    std::string handle = agent_host_->CreateIOStreamFromData(data);
    callback->sendSuccess(protocol::Binary(), handle);
  } else {
    callback->sendSuccess(protocol::Binary::fromRefCounted(data),
                          protocol::Maybe<std::string>());
  }
}
#endif  // BUILDFLAG(ENABLE_PRINTING)
