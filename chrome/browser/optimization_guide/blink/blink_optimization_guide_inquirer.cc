// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/blink/blink_optimization_guide_inquirer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/optimization_guide/blink/blink_optimization_guide_feature_flag_helper.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/optimization_guide/proto/delay_async_script_execution_metadata.pb.h"
#include "components/optimization_guide/proto/delay_competing_low_priority_requests_metadata.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"

namespace optimization_guide {

// static
std::unique_ptr<BlinkOptimizationGuideInquirer>
BlinkOptimizationGuideInquirer::CreateAndStart(
    content::NavigationHandle& navigation_handle,
    OptimizationGuideDecider& decider) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto inquirer = base::WrapUnique(new BlinkOptimizationGuideInquirer());
  inquirer->InquireHints(navigation_handle, decider);
  return inquirer;
}

BlinkOptimizationGuideInquirer::~BlinkOptimizationGuideInquirer() = default;

BlinkOptimizationGuideInquirer::BlinkOptimizationGuideInquirer()
    : optimization_guide_hints_(
          blink::mojom::BlinkOptimizationGuideHints::New()) {}

void BlinkOptimizationGuideInquirer::InquireHints(
    content::NavigationHandle& navigation_handle,
    OptimizationGuideDecider& decider) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<proto::OptimizationType> supported_optimization_types;
  if (features::ShouldUseOptimizationGuideForDelayAsyncScript()) {
    supported_optimization_types.push_back(
        proto::OptimizationType::DELAY_ASYNC_SCRIPT_EXECUTION);
  }
  if (features::
          ShouldUseOptimizationGuideForDelayCompetingLowPriorityRequests()) {
    supported_optimization_types.push_back(
        proto::OptimizationType::DELAY_COMPETING_LOW_PRIORITY_REQUESTS);
  }

  for (auto optimization_type : supported_optimization_types) {
    // CanApplyOptimizationAsync() synchronously runs the callback when the
    // hints are already available.
    decider.CanApplyOptimizationAsync(
        &navigation_handle, optimization_type,
        base::BindOnce(&BlinkOptimizationGuideInquirer::DidInquireHints,
                       weak_ptr_factory_.GetWeakPtr(), optimization_type));
  }
}

void BlinkOptimizationGuideInquirer::DidInquireHints(
    proto::OptimizationType optimization_type,
    OptimizationGuideDecision decision,
    const OptimizationMetadata& metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  switch (decision) {
    case OptimizationGuideDecision::kTrue:
      break;
    case OptimizationGuideDecision::kUnknown:
    case OptimizationGuideDecision::kFalse:
      // The optimization guide service decided not to provide the hints.
      return;
  }

  switch (optimization_type) {
    case proto::OptimizationType::DELAY_ASYNC_SCRIPT_EXECUTION:
      PopulateHintsForDelayAsyncScriptExecution(metadata);
      break;
    case proto::OptimizationType::DELAY_COMPETING_LOW_PRIORITY_REQUESTS:
      PopulateHintsForDelayCompetingLowPriorityRequests(metadata);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void BlinkOptimizationGuideInquirer::PopulateHintsForDelayAsyncScriptExecution(
    const OptimizationMetadata& optimization_metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Give up providing the hints when the metadata is not available.
  base::Optional<proto::DelayAsyncScriptExecutionMetadata> metadata =
      optimization_metadata
          .ParsedMetadata<proto::DelayAsyncScriptExecutionMetadata>();
  if (!metadata || !metadata->delay_type())
    return;

  // Populate the metadata into the hints.
  using blink::mojom::DelayAsyncScriptExecutionDelayType;
  auto hints = blink::mojom::DelayAsyncScriptExecutionHints::New();
  switch (metadata->delay_type()) {
    case proto::PerfectHeuristicsDelayType::DELAY_TYPE_UNKNOWN:
      hints->delay_type = DelayAsyncScriptExecutionDelayType::kUnknown;
      break;
    case proto::PerfectHeuristicsDelayType::DELAY_TYPE_FINISHED_PARSING:
      hints->delay_type = DelayAsyncScriptExecutionDelayType::kFinishedParsing;
      break;
    case proto::PerfectHeuristicsDelayType::
        DELAY_TYPE_FIRST_PAINT_OR_FINISHED_PARSING:
      hints->delay_type =
          DelayAsyncScriptExecutionDelayType::kFirstPaintOrFinishedParsing;
      break;
    case proto::PerfectHeuristicsDelayType::DELAY_TYPE_FIRST_PAINT:
    case proto::PerfectHeuristicsDelayType::DELAY_TYPE_FIRST_CONTENTFUL_PAINT:
      // DelayAsyncScriptExecution doesn't support these milestones.
      NOTREACHED();
      return;
  }
  DCHECK(!optimization_guide_hints_->delay_async_script_execution_hints);
  optimization_guide_hints_->delay_async_script_execution_hints =
      std::move(hints);
}

void BlinkOptimizationGuideInquirer::
    PopulateHintsForDelayCompetingLowPriorityRequests(
        const OptimizationMetadata& optimization_metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Give up providing the hints when the metadata is not available.
  base::Optional<proto::DelayCompetingLowPriorityRequestsMetadata> metadata =
      optimization_metadata
          .ParsedMetadata<proto::DelayCompetingLowPriorityRequestsMetadata>();
  if (!metadata || !metadata->delay_type() || !metadata->priority_threshold())
    return;

  // Populate the metadata into the hints.
  using ProtoDelayType = proto::PerfectHeuristicsDelayType;
  using MojomDelayType =
      blink::mojom::DelayCompetingLowPriorityRequestsDelayType;
  auto hints = blink::mojom::DelayCompetingLowPriorityRequestsHints::New();
  switch (metadata->delay_type()) {
    case ProtoDelayType::DELAY_TYPE_UNKNOWN:
      hints->delay_type = MojomDelayType::kUnknown;
      break;
    case ProtoDelayType::DELAY_TYPE_FIRST_PAINT:
      hints->delay_type = MojomDelayType::kFirstPaint;
      break;
    case ProtoDelayType::DELAY_TYPE_FIRST_CONTENTFUL_PAINT:
      hints->delay_type = MojomDelayType::kFirstContentfulPaint;
      break;
    case ProtoDelayType::DELAY_TYPE_FINISHED_PARSING:
    case ProtoDelayType::DELAY_TYPE_FIRST_PAINT_OR_FINISHED_PARSING:
      // DelayCompetingLowPriorityRequests doesn't support these milestones.
      NOTREACHED();
      return;
  }
  using MojomPriorityThreshold =
      blink::mojom::DelayCompetingLowPriorityRequestsPriorityThreshold;
  switch (metadata->priority_threshold()) {
    case proto::PriorityThreshold::PRIORITY_THRESHOLD_UNKNOWN:
      hints->priority_threshold = MojomPriorityThreshold::kUnknown;
      break;
    case proto::PriorityThreshold::PRIORITY_THRESHOLD_MEDIUM:
      hints->priority_threshold = MojomPriorityThreshold::kMedium;
      break;
    case proto::PriorityThreshold::PRIORITY_THRESHOLD_HIGH:
      hints->priority_threshold = MojomPriorityThreshold::kHigh;
      break;
  }
  DCHECK(
      !optimization_guide_hints_->delay_competing_low_priority_requests_hints);
  optimization_guide_hints_->delay_competing_low_priority_requests_hints =
      std::move(hints);
}

}  // namespace optimization_guide
