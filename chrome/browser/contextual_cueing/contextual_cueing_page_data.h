// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_PAGE_DATA_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_PAGE_DATA_H_

#include "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#include "components/pdf/common/constants.h"
#include "content/public/browser/page_user_data.h"
#include "pdf/buildflags.h"
#include "pdf/mojom/pdf.mojom.h"

namespace contextual_cueing {

// Decider for contextual cueing that is scoped to `Page`.
class ContextualCueingPageData
    : public content::PageUserData<ContextualCueingPageData> {
 public:
  using CueingDecisionCallback = base::OnceCallback<void(const std::string&)>;

  ContextualCueingPageData(const ContextualCueingPageData&) = delete;
  ContextualCueingPageData& operator=(const ContextualCueingPageData&) = delete;
  ~ContextualCueingPageData() override;

 private:
  friend class content::PageUserData<ContextualCueingPageData>;
  friend class ContextualCueingPageDataTest;

  // Holds the cueing condition decision for one cueing configuration.
  enum CueingConfigurationDecision {
    kAllowed,
    kDisallowed,
    kNeedsPdfPageCount,
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
  void RequestPdfPageCount();

  void OnPdfPageCountReceived(pdf::mojom::PdfListener::GetPdfBytesStatus status,
                              const std::vector<uint8_t>& bytes,
                              uint32_t page_count);
#endif  // BUILDFLAG(ENABLE_PDF)

  const optimization_guide::proto::GlicContextualCueingMetadata metadata_;

  // Holds the page count of PDF. Populated only when the mainframe of the page
  // has a PDF renderer, and the page count has been successfully retrieved from
  // it.
  std::optional<size_t> pdf_page_count_;

  CueingDecisionCallback cueing_decision_callback_;

  base::WeakPtrFactory<ContextualCueingPageData> weak_factory_{this};

  PAGE_USER_DATA_KEY_DECL();
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_PAGE_DATA_H_
