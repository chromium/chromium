// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/blink/blink_optimization_guide_inquirer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/optimization_guide/blink/blink_optimization_guide_feature_flag_helper.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/optimization_guide/proto/delay_async_script_execution_metadata.pb.h"
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
    default:
      NOTREACHED();
      break;
  }
}

void BlinkOptimizationGuideInquirer::PopulateHintsForDelayAsyncScriptExecution(
    const OptimizationMetadata& optimization_metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Give up providing the hints when the metadata is not available.
  absl::optional<proto::DelayAsyncScriptExecutionMetadata> metadata =
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

}  // namespace optimization_guide
