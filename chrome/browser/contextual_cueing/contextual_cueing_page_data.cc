// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_page_data.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "content/public/browser/web_contents.h"
#include "pdf/buildflags.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace contextual_cueing {

namespace {

bool DidMatchCueingCondition(
    const optimization_guide::proto::ContextualCueingConditions& condition,
    int64_t value) {
  if (!condition.has_cueing_operator()) {
    return false;
  }
  if (!condition.has_int64_threshold()) {
    return false;
  }
  switch (condition.cueing_operator()) {
    case optimization_guide::proto::CONTEXTUAL_CUEING_OPERATOR_UNSPECIFIED:
      return false;
    case optimization_guide::proto::
        CONTEXTUAL_CUEING_OPERATOR_GREATER_THAN_OR_EQUAL_TO:
      return value >= condition.int64_threshold();
    case optimization_guide::proto::
        CONTEXTUAL_CUEING_OPERATOR_LESS_THAN_OR_EQUAL_TO:
      return value <= condition.int64_threshold();
  }
}

}  // namespace

ContextualCueingPageData::ContextualCueingPageData(
    content::Page& page,
    optimization_guide::proto::GlicContextualCueingMetadata metadata,
    CueingDecisionCallback cueing_decision_callback)
    : content::PageUserData<ContextualCueingPageData>(page),
      metadata_(metadata),
      cueing_decision_callback_(std::move(cueing_decision_callback)) {
  FindMatchingConfig();
}

ContextualCueingPageData::~ContextualCueingPageData() = default;

PAGE_USER_DATA_KEY_IMPL(ContextualCueingPageData);

// Attempts to find the matching cueing configuration.
void ContextualCueingPageData::FindMatchingConfig() {
  CHECK(cueing_decision_callback_);
  bool needs_pdf_page_count = false;
  for (const auto& config : metadata_.cueing_configurations()) {
    if (!config.has_cue_label()) {
      continue;
    }
    auto decision = DidMatchCueingConditions(config);
    if (decision == kAllowed) {
      std::move(cueing_decision_callback_).Run(config.cue_label());
      return;
    }
    if (decision == kNeedsPdfPageCount) {
      needs_pdf_page_count = true;
    }
  }
  if (needs_pdf_page_count) {
#if BUILDFLAG(ENABLE_PDF)
    CHECK_EQ(pdf::kPDFMimeType, page().GetContentsMimeType());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ContextualCueingPageData::RequestPdfPageCount,
                       weak_factory_.GetWeakPtr()),
        kPdfPageCountCaptureDelay.Get());
    return;
#endif  // BUILDFLAG(ENABLE_PDF)
  }
  // None of the config matched, and no client-signals were requested.
  std::move(cueing_decision_callback_).Run(std::string());
}

ContextualCueingPageData::CueingConfigurationDecision
ContextualCueingPageData::DidMatchCueingConditions(
    const optimization_guide::proto::GlicCueingConfiguration& config) {
  for (const auto& condition : config.conditions()) {
    switch (condition.signal()) {
      case optimization_guide::proto::
          CONTEXTUAL_CUEING_CLIENT_SIGNAL_UNSPECIFIED:
        return kDisallowed;
      case optimization_guide::proto::
          CONTEXTUAL_CUEING_CLIENT_SIGNAL_PDF_PAGE_COUNT:
        if (page().GetContentsMimeType() != pdf::kPDFMimeType) {
          return kDisallowed;
        }
        if (!pdf_page_count_) {
          return kNeedsPdfPageCount;
        }
        return DidMatchCueingCondition(condition, *pdf_page_count_)
                   ? kAllowed
                   : kDisallowed;
      case optimization_guide::proto::
          CONTEXTUAL_CUEING_CLIENT_SIGNAL_CONTENT_LENGTH_WORD_COUNT:
        // TODO: crbug.com/389751174 - Implement checking the client signals.
        return kDisallowed;
    }
  }
  return kAllowed;
}

#if BUILDFLAG(ENABLE_PDF)
void ContextualCueingPageData::RequestPdfPageCount() {
  CHECK(page().GetContentsMimeType() == pdf::kPDFMimeType);
  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::MaybeGetForWebContents(
          content::WebContents::FromRenderFrameHost(&page().GetMainDocument()));
  if (pdf_helper) {
    // Fetch zero PDF bytes to just receive the total page count.
    pdf_helper->GetPdfBytes(
        /*size_limit=*/0,
        base::BindOnce(&ContextualCueingPageData::OnPdfPageCountReceived,
                       weak_factory_.GetWeakPtr()));
  }
}

void ContextualCueingPageData::OnPdfPageCountReceived(
    pdf::mojom::PdfListener::GetPdfBytesStatus status,
    const std::vector<uint8_t>& bytes,
    uint32_t page_count) {
  if (status == pdf::mojom::PdfListener::GetPdfBytesStatus::kFailed) {
    return;
  }
  pdf_page_count_ = page_count;
  FindMatchingConfig();
}
#endif  // BUILDFLAG(ENABLE_PDF)

}  // namespace contextual_cueing
