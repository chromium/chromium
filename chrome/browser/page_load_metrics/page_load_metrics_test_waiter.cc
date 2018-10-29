// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/page_load_metrics_test_waiter.h"

#include "base/logging.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/common/page_load_metrics/page_load_metrics.mojom.h"
#include "content/public/common/resource_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

PageLoadMetricsTestWaiter::PageLoadMetricsTestWaiter(
    content::WebContents* web_contents)
    : TestingObserver(web_contents), weak_factory_(this) {}

PageLoadMetricsTestWaiter::~PageLoadMetricsTestWaiter() {
  CHECK(did_add_observer_);
  CHECK_EQ(nullptr, run_loop_.get());
}

void PageLoadMetricsTestWaiter::AddPageExpectation(TimingField field) {
  page_expected_fields_.Set(field);
  if (field == TimingField::kLoadTimingInfo) {
    attach_on_tracker_creation_ = true;
  }
}

void PageLoadMetricsTestWaiter::AddSubFrameExpectation(TimingField field) {
  CHECK_NE(field, TimingField::kLoadTimingInfo)
      << "LOAD_TIMING_INFO should only be used as a page-level expectation";
  subframe_expected_fields_.Set(field);
  // If the given field is also a page-level field, then add a page-level
  // expectation as well
  if (IsPageLevelField(field))
    page_expected_fields_.Set(field);
}

void PageLoadMetricsTestWaiter::AddMinimumCompleteResourcesExpectation(
    int expected_minimum_complete_resources) {
  expected_minimum_complete_resources_ = expected_minimum_complete_resources;
}

void PageLoadMetricsTestWaiter::AddMinimumResourceBytesExpectation(
    int expected_minimum_resource_bytes) {
  expected_minimum_resource_bytes_ = expected_minimum_resource_bytes;
}

bool PageLoadMetricsTestWaiter::DidObserveInPage(TimingField field) const {
  return observed_page_fields_.IsSet(field);
}

void PageLoadMetricsTestWaiter::Wait() {
  if (ExpectationsSatisfied())
    return;

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_ = nullptr;

  EXPECT_TRUE(ExpectationsSatisfied());
}

void PageLoadMetricsTestWaiter::OnTimingUpdated(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (ExpectationsSatisfied())
    return;
  const page_load_metrics::mojom::PageLoadMetadata& metadata =
      subframe_rfh ? extra_info.subframe_metadata
                   : extra_info.main_frame_metadata;
  TimingFieldBitSet matched_bits = GetMatchedBits(timing, metadata);
  if (subframe_rfh) {
    subframe_expected_fields_.ClearMatching(matched_bits);
  } else {
    page_expected_fields_.ClearMatching(matched_bits);
    observed_page_fields_.Merge(matched_bits);
  }
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  if (ExpectationsSatisfied())
    return;

  if (extra_request_complete_info.resource_type !=
      content::RESOURCE_TYPE_MAIN_FRAME) {
    // The waiter confirms loading timing for the main frame only.
    return;
  }

  if (!extra_request_complete_info.load_timing_info->send_start.is_null() &&
      !extra_request_complete_info.load_timing_info->send_end.is_null() &&
      !extra_request_complete_info.load_timing_info->request_start.is_null()) {
    page_expected_fields_.Clear(TimingField::kLoadTimingInfo);
    observed_page_fields_.Set(TimingField::kLoadTimingInfo);
  }
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnResourceDataUseObserved(
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  for (auto const& resource : resources) {
    auto it = page_resources_.find(resource->request_id);
    if (it != page_resources_.end()) {
      it->second = resource.Clone();
    } else {
      page_resources_.emplace(std::piecewise_construct,
                              std::forward_as_tuple(resource->request_id),
                              std::forward_as_tuple(resource->Clone()));
    }
    if (resource->is_complete)
      current_complete_resources_++;
    current_resource_bytes_ += resource->delta_bytes;
  }
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

bool PageLoadMetricsTestWaiter::IsPageLevelField(TimingField field) {
  switch (field) {
    case TimingField::kFirstPaint:
    case TimingField::kFirstContentfulPaint:
    case TimingField::kFirstMeaningfulPaint:
      return true;
    default:
      return false;
  }
}

PageLoadMetricsTestWaiter::TimingFieldBitSet
PageLoadMetricsTestWaiter::GetMatchedBits(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::mojom::PageLoadMetadata& metadata) {
  PageLoadMetricsTestWaiter::TimingFieldBitSet matched_bits;
  if (timing.document_timing->first_layout)
    matched_bits.Set(TimingField::kFirstLayout);
  if (timing.document_timing->load_event_start)
    matched_bits.Set(TimingField::kLoadEvent);
  if (timing.paint_timing->first_paint)
    matched_bits.Set(TimingField::kFirstPaint);
  if (timing.paint_timing->first_contentful_paint)
    matched_bits.Set(TimingField::kFirstContentfulPaint);
  if (timing.paint_timing->first_meaningful_paint)
    matched_bits.Set(TimingField::kFirstMeaningfulPaint);
  if (metadata.behavior_flags & blink::WebLoadingBehaviorFlag::
                                    kWebLoadingBehaviorDocumentWriteBlockReload)
    matched_bits.Set(TimingField::kDocumentWriteBlockReload);

  return matched_bits;
}

void PageLoadMetricsTestWaiter::OnTrackerCreated(
    page_load_metrics::PageLoadTracker* tracker) {
  if (!attach_on_tracker_creation_)
    return;
  // A PageLoadMetricsWaiter should only wait for events from a single page
  // load.
  ASSERT_FALSE(did_add_observer_);
  tracker->AddObserver(
      std::make_unique<WaiterMetricsObserver>(weak_factory_.GetWeakPtr()));
  did_add_observer_ = true;
}

void PageLoadMetricsTestWaiter::OnCommit(
    page_load_metrics::PageLoadTracker* tracker) {
  if (attach_on_tracker_creation_)
    return;
  // A PageLoadMetricsWaiter should only wait for events from a single page
  // load.
  ASSERT_FALSE(did_add_observer_);
  tracker->AddObserver(
      std::make_unique<WaiterMetricsObserver>(weak_factory_.GetWeakPtr()));
  did_add_observer_ = true;
}

bool PageLoadMetricsTestWaiter::ResourceUseExpectationsSatisfied() const {
  return (expected_minimum_complete_resources_ == 0 ||
          current_complete_resources_ >=
              expected_minimum_complete_resources_) &&
         (expected_minimum_resource_bytes_ == 0 ||
          current_resource_bytes_ >= expected_minimum_resource_bytes_);
}

bool PageLoadMetricsTestWaiter::ExpectationsSatisfied() const {
  return subframe_expected_fields_.Empty() && page_expected_fields_.Empty() &&
         ResourceUseExpectationsSatisfied();
}

PageLoadMetricsTestWaiter::WaiterMetricsObserver::~WaiterMetricsObserver() {}

PageLoadMetricsTestWaiter::WaiterMetricsObserver::WaiterMetricsObserver(
    base::WeakPtr<PageLoadMetricsTestWaiter> waiter)
    : waiter_(waiter) {}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (waiter_)
    waiter_->OnTimingUpdated(subframe_rfh, timing, extra_info);
}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  if (waiter_)
    waiter_->OnLoadedResource(extra_request_complete_info);
}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::
    OnResourceDataUseObserved(
        const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
            resources) {
  if (waiter_)
    waiter_->OnResourceDataUseObserved(resources);
}

}  // namespace page_load_metrics
