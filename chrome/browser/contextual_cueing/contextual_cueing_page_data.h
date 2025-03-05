// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_PAGE_DATA_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_PAGE_DATA_H_

#include "base/types/expected.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/pdf/common/constants.h"
#include "content/public/browser/page_user_data.h"
#include "pdf/buildflags.h"
#include "pdf/mojom/pdf.mojom.h"

namespace contextual_cueing {

// Decider for contextual cueing that is scoped to `Page`.
class ContextualCueingPageData
    : public content::PageUserData<ContextualCueingPageData> {
 public:
  using CueingDecisionCallback =
      base::OnceCallback<void(base::expected<std::string, NudgeDecision>)>;

  ContextualCueingPageData(const ContextualCueingPageData&) = delete;
  ContextualCueingPageData& operator=(const ContextualCueingPageData&) = delete;
  ~ContextualCueingPageData() override;

  void OnPageContentExtracted(
      const optimization_guide::proto::AnnotatedPageContent& page_content);

 private:
  friend class content::PageUserData<ContextualCueingPageData>;
  friend class ContextualCueingPageDataTest;

  // Holds the cueing condition decision for one cueing configuration.
  enum CueingConfigurationDecision {
    kAllowed,
    kDisallowed,
    kNeedsPdfPageCount,
    kNeedsPageContent,
  };

  // Holds the info related to word count client signal.
  struct WordCountInfo {
    // Maximum word count needed for the cueing conditions. This limits
    // iterating through the page content more than needed.
    size_t max_count_needed = 0;

    // Count of words calculated from the page content. This is always less than
    // or equal to `max_count_needed`. When the page count is more than the
    // limit, the counting is stopped at the limit.
    std::optional<size_t> page_contents_words;
  };

  ContextualCueingPageData(
      content::Page& page,
      optimization_guide::proto::GlicContextualCueingMetadata metadata,
      CueingDecisionCallback cueing_decision_callback);

  // Finds the matching config that passes all the conditions. Requests and
  // waits for any client-signals when needed.
  void FindMatchingConfig();

  // Returns whether the `config` matches all the current cueing condition.
  CueingConfigurationDecision DidMatchCueingConditions(
      const optimization_guide::proto::GlicCueingConfiguration& config);

#if BUILDFLAG(ENABLE_PDF)
  // Requests for page count if this is a PDF page.
  void RequestPdfPageCount();

  // Invoked when PDF document is loaded, so that the metadata can be queried.
  void OnPdfDocumentLoadComplete();

  // Invoked when page count is received.
  void OnPdfPageCountReceived(pdf::mojom::PdfListener::GetPdfBytesStatus status,
                              const std::vector<uint8_t>& bytes,
                              uint32_t page_count);
#endif  // BUILDFLAG(ENABLE_PDF)

  const optimization_guide::proto::GlicContextualCueingMetadata metadata_;

  // Holds the page count of PDF. Populated only when the mainframe of the page
  // has a PDF renderer, and the page count has been successfully retrieved from
  // it.
  std::optional<size_t> pdf_page_count_;

  // Holds the word count info. Populated only when word count signal is
  // required for making the cueing decision.
  std::optional<WordCountInfo> page_content_word_count_info_;

  CueingDecisionCallback cueing_decision_callback_;

  base::WeakPtrFactory<ContextualCueingPageData> weak_factory_{this};

  PAGE_USER_DATA_KEY_DECL();
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_PAGE_DATA_H_
