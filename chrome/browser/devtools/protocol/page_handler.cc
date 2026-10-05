// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/page_handler.h"

#include <string>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/functional/bind.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/printing/browser/headless/headless_print_manager.h"
#include "components/printing/browser/print_to_pdf/pdf_print_utils.h"
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINTING)

#if BUILDFLAG(ENABLE_PRINTING)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
using ActivePrintManager = printing::PrintViewManager;
#else
using ActivePrintManager = printing::PrintViewManagerBasic;
#endif

#endif  // BUILDFLAG(ENABLE_PRINTING)

void PageHandler::PrintToPDF(std::optional<bool> landscape,
                             std::optional<bool> display_header_footer,
                             std::optional<bool> print_background,
                             std::optional<double> scale,
                             std::optional<double> paper_width,
                             std::optional<double> paper_height,
                             std::optional<double> margin_top,
                             std::optional<double> margin_bottom,
                             std::optional<double> margin_left,
                             std::optional<double> margin_right,
                             std::optional<protocol::String> page_ranges,
                             std::optional<protocol::String> header_template,
                             std::optional<protocol::String> footer_template,
                             std::optional<bool> prefer_css_page_size,
                             std::optional<protocol::String> transfer_mode,
                             std::optional<bool> generate_tagged_pdf,
                             std::optional<bool> generate_document_outline,
                             std::unique_ptr<PrintToPDFCallback> callback) {
  CHECK(callback, base::NotFatalUntil::M161);

#if BUILDFLAG(ENABLE_PRINTING)
  if (!web_contents_) {
    callback->sendFailure(
        protocol::Response::ServerError("No web contents to print"));
    return;
  }

  std::variant<printing::mojom::PrintPagesParamsPtr, std::string>
      print_pages_params = print_to_pdf::GetPrintPagesParams(
          web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(),
          landscape, display_header_footer, print_background, scale,
          paper_width, paper_height, margin_top, margin_bottom, margin_left,
          margin_right, header_template, footer_template, prefer_css_page_size,
          generate_tagged_pdf, generate_document_outline);
  if (std::holds_alternative<std::string>(print_pages_params)) {
    callback->sendFailure(protocol::Response::InvalidParams(
        std::get<std::string>(print_pages_params)));
    return;
  }

  CHECK(std::holds_alternative<printing::mojom::PrintPagesParamsPtr>(
            print_pages_params),
        base::NotFatalUntil::M161);

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
        std::move(
            std::get<printing::mojom::PrintPagesParamsPtr>(print_pages_params)),
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
        std::move(
            std::get<printing::mojom::PrintPagesParamsPtr>(print_pages_params)),
        base::BindOnce(&PageHandler::OnPDFCreated,
                       weak_ptr_factory_.GetWeakPtr(), return_as_stream,
                       std::move(callback)));
    return;
  }
#endif  // BUILDFLAG(ENABLE_PRINTING)

  callback->sendFailure(
      protocol::Response::ServerError("Printing is not available"));
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
    callback->sendSuccess(protocol::Binary::fromRefCounted(data), std::nullopt);
  }
}
#endif  // BUILDFLAG(ENABLE_PRINTING)
