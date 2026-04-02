// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"

#include <memory>
#include <optional>

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/optimization_guide/core/optimization_guide_common.mojom.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"

namespace contextual_cueing {

namespace {

// Convenience macro for emitting OPTIMIZATION_GUIDE_LOGs where
// optimization_keyed_service_ is defined.
#define MODEL_EXECUTION_LOG(message)                                   \
  OPTIMIZATION_GUIDE_LOG(                                              \
      optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,    \
      optimization_guide_keyed_service_->GetOptimizationGuideLogger(), \
      (message))

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
      tab_list_interface_(tab_list_interface),
      page_content_annotations_service_(
          PageContentAnnotationsServiceFactory::GetForProfile(
              browser_window_interface_->GetProfile())),
      optimization_guide_keyed_service_(
          OptimizationGuideKeyedServiceFactory::GetForProfile(
              browser_window_interface_->GetProfile())) {
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
    MODEL_EXECUTION_LOG(
        base::StringPrintf("%s ineligible for cue: No longer active tab after "
                           "category classification.",
                           active_web_contents->GetLastCommittedURL().spec()));
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
    MODEL_EXECUTION_LOG(base::StringPrintf(
        "%s ineligible for cue: Failed category classification.",
        active_web_contents->GetLastCommittedURL().spec()));
    RecordContextualCueingDecision(
        ContextualCueingDecision::kFailedCategoryClassification);
    return;
  }

  MODEL_EXECUTION_LOG(
      base::StringPrintf("%s eligible for cue: Category classification "
                         "succeeded. Initiating model execution request.",
                         active_web_contents->GetLastCommittedURL().spec()));
  InitiateModelExecutionRequest();
}

void ContextualCueingController::InitiateModelExecutionRequest() {
  if (!optimization_guide_keyed_service_) {
    RecordContextualCueingDecision(
        ContextualCueingDecision::kModelExecutionUnavailable);
    return;
  }

  optimization_guide::proto::ContextualCueingRequest request;
  content::WebContents* active_web_contents =
      tab_list_interface_->GetActiveTab()
          ? tab_list_interface_->GetActiveTab()->GetContents()
          : nullptr;
  CHECK(active_web_contents);
  request.mutable_active_tab_page_context()->set_url(
      active_web_contents->GetLastCommittedURL().spec());
  request.mutable_active_tab_page_context()->set_title(
      base::UTF16ToUTF8(active_web_contents->GetTitle()));
  optimization_guide_keyed_service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kContextualCueing, request,
      /*options=*/{},
      base::BindOnce(
          &ContextualCueingController::OnModelExecutionResponseReceived,
          weak_ptr_factory_.GetWeakPtr()));
}

void ContextualCueingController::OnModelExecutionResponseReceived(
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  // TODO: b/496000131 - Ensure we are still on the same tab that we requested
  // for.

  if (!result.response.has_value()) {
    MODEL_EXECUTION_LOG(
        base::StringPrintf("Model execution to generate cue failed."));
    RecordContextualCueingDecision(
        ContextualCueingDecision::kModelExecutionFailed);
    return;
  }

  std::optional<optimization_guide::proto::ContextualCueingResponse> response =
      optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::ContextualCueingResponse>(
          *result.response);
  if (!response) {
    MODEL_EXECUTION_LOG(
        base::StringPrintf("Model execution to generate cue failed."));
    RecordContextualCueingDecision(
        ContextualCueingDecision::kModelExecutionResponseFailedToParse);
    return;
  }

  if (response->has_anchored_message_cue()) {
    MODEL_EXECUTION_LOG(base::StringPrintf(
        "Showing cue for CUJ %s: [%s] %s", response->suggested_cuj(),
        response->anchored_message_cue().anchored_message_text(),
        response->anchored_message_cue().action_text()));
  }
  RecordContextualCueingDecision(ContextualCueingDecision::kSuccess);
}

}  // namespace contextual_cueing
