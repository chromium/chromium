// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CONTROLLER_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"

class BrowserWindowInterface;
class TabListInterface;

namespace contextual_cueing {

enum class ContextualCueingDecision {
  kUnspecified = 0,
  // Tab was not active when the page was classified.
  kNoLongerActiveTabAfterCategoryClassification = 1,
  // Tab was active but the page was not classified as a vertical we support.
  kFailedCategoryClassification = 2,

  kMaxValue = kFailedCategoryClassification,
};

class ContextualCueingController
    : public page_content_annotations::PageContentAnnotationsService::
          PageContentAnnotationsObserver {
 public:
  explicit ContextualCueingController(
      BrowserWindowInterface* browser_window_interface,
      TabListInterface* tab_list_interface);
  ContextualCueingController(const ContextualCueingController&) = delete;
  ContextualCueingController& operator=(const ContextualCueingController&) =
      delete;
  ~ContextualCueingController() override;

  // page_content_annotations::PageContentAnnotationsService::
  // PageContentAnnotationsServiceObserver:
  void OnPageContentAnnotated(
      const page_content_annotations::HistoryVisit& visit,
      const page_content_annotations::PageContentAnnotationsResult& result)
      override;

 private:
  // Not owned. Guaranteed to outlive `this`.
  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
  const raw_ptr<TabListInterface> tab_list_interface_;
  raw_ptr<page_content_annotations::PageContentAnnotationsService>
      page_content_annotations_service_;
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CONTROLLER_H_
