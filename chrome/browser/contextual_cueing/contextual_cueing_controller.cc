// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"

namespace contextual_cueing {

namespace {

void RecordContextualCueingDecision(
    ContextualCueingDecision contextual_cueing_decision) {
  base::UmaHistogramEnumeration("ContextualCueing.V2.Decision",
                                contextual_cueing_decision);
}

}  // namespace

ContextualCueingController::ContextualCueingController(
    BrowserWindowInterface* browser_window_interface,
    TabListInterface* tab_list_interface)
    : browser_window_interface_(browser_window_interface),
      tab_list_interface_(tab_list_interface) {
  page_content_annotations_service_ =
      PageContentAnnotationsServiceFactory::GetForProfile(
          browser_window_interface_->GetProfile());
  if (page_content_annotations_service_) {
    page_content_annotations_service_->AddObserver(
        page_content_annotations::AnnotationType::kCategoryClassifier, this);
  }
}

ContextualCueingController::~ContextualCueingController() {
  if (page_content_annotations_service_) {
    page_content_annotations_service_->RemoveObserver(
        page_content_annotations::AnnotationType::kCategoryClassifier, this);
  }
}

void ContextualCueingController::OnPageContentAnnotated(
    const page_content_annotations::HistoryVisit& visit,
    const page_content_annotations::PageContentAnnotationsResult& result) {
  content::WebContents* active_web_contents =
      tab_list_interface_->GetActiveTab()
          ? tab_list_interface_->GetActiveTab()->GetContents()
          : nullptr;
  if (!active_web_contents ||
      visit.nav_entry_timestamp != active_web_contents->GetController()
                                       .GetLastCommittedEntry()
                                       ->GetTimestamp()) {
    RecordContextualCueingDecision(
        ContextualCueingDecision::
            kNoLongerActiveTabAfterCategoryClassification);
    return;
  }

  // Check classification to see if we should proceed to next step.
  bool is_supported_category = false;
  for (const page_content_annotations::Category& category :
       result.GetCategoryResults()) {
    if (category.category_type ==
            page_content_annotations::CategoryType::kEducation &&
        category.score > kEduClassifierThreshold.Get()) {
      is_supported_category = true;
      break;
    }
    if (category.category_type ==
            page_content_annotations::CategoryType::kShopping &&
        category.score > kShoppingClassifierThreshold.Get()) {
      is_supported_category = true;
      break;
    }
  }

  if (!is_supported_category) {
    RecordContextualCueingDecision(
        ContextualCueingDecision::kFailedCategoryClassification);
    return;
  }

  // TODO: b/496000131 - Initiate call to MES.
}

}  // namespace contextual_cueing
