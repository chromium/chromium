// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_tuning_utils.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace performance_manager::mechanism {
namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(DiscardPageOnUIThreadOutcome)
enum class DiscardPageOnUIThreadOutcome {
  kSuccess = 0,
  kNoContents = 1,
  kDiscardTabFailure = 2,
  kMaxValue = kDiscardTabFailure
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/tab/enums.xml:DiscardPageOnUIThreadOutcome)

}  // namespace

std::optional<uint64_t> PageDiscarder::DiscardPageNode(
    const PageNode* page_node,
    resource_coordinator::LifecycleUnitDiscardReason discard_reason) {
  base::WeakPtr<content::WebContents> contents = page_node->GetWebContents();

  std::optional<DiscardPageOnUIThreadOutcome> outcome;
  absl::Cleanup record_discard_outcome = [&]() {
    CHECK(outcome.has_value(), base::NotFatalUntil::M140);
    base::UmaHistogramEnumeration("Discarding.DiscardPageOnUIThreadOutcome",
                                  outcome.value());
  };

  CHECK(contents, base::NotFatalUntil::M140);
  if (!contents) {
    outcome = DiscardPageOnUIThreadOutcome::kNoContents;
    return std::nullopt;
  }

  auto* lifecycle_unit =
      resource_coordinator::TabLifecycleUnitSource::GetTabLifecycleUnitExternal(
          contents.get());
  // This function is only called with `PageNode`s of type `kTab`, so there
  // should be a LifecycleUnit.
  CHECK(lifecycle_unit, base::NotFatalUntil::M140);

  uint64_t memory_footprint_estimate =
      user_tuning::GetDiscardedMemoryEstimateForPage(page_node);

  if (!lifecycle_unit->DiscardTab(discard_reason, memory_footprint_estimate)) {
    outcome = DiscardPageOnUIThreadOutcome::kDiscardTabFailure;
    return std::nullopt;
  }

  outcome = DiscardPageOnUIThreadOutcome::kSuccess;
  return memory_footprint_estimate;
}

}  // namespace performance_manager::mechanism
