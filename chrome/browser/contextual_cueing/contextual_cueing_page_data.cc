// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_page_data.h"

#include "base/i18n/char_iterator.h"
#include "base/strings/string_util.h"
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

void CountWords(const optimization_guide::proto::ContentNode& content_node,
                size_t max_word_count_limit,
                size_t* word_count) {
  bool is_previous_char_whitespace = true;
  for (base::i18n::UTF8CharIterator iter(
           content_node.content_attributes().text_data().text_content());
       *word_count < max_word_count_limit && !iter.end(); iter.Advance()) {
    bool is_current_char_whitespace = base::IsUnicodeWhitespace(iter.get());
    if (is_previous_char_whitespace && !is_current_char_whitespace) {
      // Count the start of the word.
      ++*word_count;
    }
    is_previous_char_whitespace = is_current_char_whitespace;
  }

  for (const auto& child : content_node.children_nodes()) {
    CountWords(child, max_word_count_limit, word_count);
  }
}

}  // namespace

ContextualCueingPageData::ContextualCueingPageData(
    content::Page& page,
    optimization_guide::proto::GlicContextualCueingMetadata metadata,
    CueingDecisionCallback cueing_decision_callback)
    : content::PageUserData<ContextualCueingPageData>(page),
      metadata_(std::move(metadata)),
      cueing_decision_callback_(std::move(cueing_decision_callback)) {
  FindMatchingConfig();
}

ContextualCueingPageData::~ContextualCueingPageData() {
  if (cueing_decision_callback_) {
    std::move(cueing_decision_callback_)
        .Run(base::unexpected(NudgeDecision::kNudgeDecisionInterrupted));
  }
}

PAGE_USER_DATA_KEY_IMPL(ContextualCueingPageData);

// Attempts to find the matching cueing configuration.
void ContextualCueingPageData::FindMatchingConfig() {
  CHECK(cueing_decision_callback_);
  bool needs_pdf_page_count = false;
  bool needs_page_content = false;
  for (const auto& config : metadata_.cueing_configurations()) {
    if (!config.has_cue_label()) {
      continue;
    }
    auto decision = DidMatchCueingConditions(config);
    if (decision == kAllowed) {
      if (kUseDynamicCues.Get() && config.has_dynamic_cue_label()) {
        std::move(cueing_decision_callback_)
            .Run(base::ok(CueingResult{config.dynamic_cue_label(),
                                       config.default_text(),
                                       /*is_dynamic=*/true}));
      } else {
        std::move(cueing_decision_callback_)
            .Run(base::ok(CueingResult{config.cue_label(),
                                       /*prompt_suggestion=*/"",
                                       /*is_dynamic=*/false}));
      }
      return;
    } else if (decision == kNeedsPdfPageCount) {
      needs_pdf_page_count = true;
    } else if (decision == kNeedsPageContent) {
      needs_page_content = true;
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
  if (needs_page_content) {
    // Wait till the page content is returned.
    return;
  }
  // None of the config matched, and no client-signals were requested.
  std::move(cueing_decision_callback_)
      .Run(base::unexpected(NudgeDecision::kClientConditionsUnmet));
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
        if (page().GetContentsMimeType() == pdf::kPDFMimeType) {
          return kDisallowed;
        }
        if (page_content_word_count_info_ &&
            page_content_word_count_info_->page_contents_words) {
          return DidMatchCueingCondition(
                     condition,
                     *page_content_word_count_info_->page_contents_words)
                     ? kAllowed
                     : kDisallowed;
        }
        if (!page_content_word_count_info_) {
          page_content_word_count_info_ = {.max_count_needed = 0};
        }
        if (page_content_word_count_info_->max_count_needed <
            static_cast<size_t>(condition.int64_threshold())) {
          page_content_word_count_info_->max_count_needed =
              static_cast<size_t>(condition.int64_threshold()) + 1;
        }
        return kNeedsPageContent;
    }
  }
  return kAllowed;
}

#if BUILDFLAG(ENABLE_PDF)
void ContextualCueingPageData::RequestPdfPageCount() {
  CHECK_EQ(pdf::kPDFMimeType, page().GetContentsMimeType());

  auto* pdf_helper = pdf::PDFDocumentHelper::MaybeGetForWebContents(
      content::WebContents::FromRenderFrameHost(&page().GetMainDocument()));
  if (pdf_helper) {
    pdf_helper->RegisterForDocumentLoadComplete(
        base::BindOnce(&ContextualCueingPageData::OnPdfDocumentLoadComplete,
                       weak_factory_.GetWeakPtr()));
  }
}

void ContextualCueingPageData::OnPdfDocumentLoadComplete() {
  CHECK_EQ(pdf::kPDFMimeType, page().GetContentsMimeType());
  auto* pdf_helper = pdf::PDFDocumentHelper::MaybeGetForWebContents(
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

void ContextualCueingPageData::OnPageContentExtracted(
    const optimization_guide::proto::AnnotatedPageContent& page_content) {
  if (!cueing_decision_callback_) {
    return;
  }
  if (!page_content_word_count_info_) {
    return;
  }
  size_t word_count = 0;
  CountWords(page_content.root_node(),
             page_content_word_count_info_->max_count_needed, &word_count);
  page_content_word_count_info_->page_contents_words = word_count;
  FindMatchingConfig();
}

}  // namespace contextual_cueing
