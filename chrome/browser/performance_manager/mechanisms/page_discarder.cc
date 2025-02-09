// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_tuning_utils.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace performance_manager {
namespace mechanism {
namespace {

using WebContentsAndPmf =
    std::pair<base::WeakPtr<content::WebContents>, uint64_t>;

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

// Discards pages. Returns a DiscardEvent for each successful discard.
// TODO(crbug.com/40194498): Returns the remaining reclaim target so
// UrgentlyDiscardMultiplePages can keep reclaiming until the reclaim target is
// met or there is no discardable page.
std::vector<PageDiscarder::DiscardEvent> DiscardPagesImpl(
    const std::vector<WebContentsAndPmf>& web_contents_and_pmf,
    resource_coordinator::LifecycleUnitDiscardReason discard_reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<PageDiscarder::DiscardEvent> discard_events;

  for (const auto& [contents, memory_footprint_estimate] :
       web_contents_and_pmf) {
    // On scope exit, `outcome` is reported to the histogram
    // "Discarding.DiscardPageOnUIThreadOutcome".
    std::optional<DiscardPageOnUIThreadOutcome> outcome;
    absl::Cleanup record_discard_outcome = [&]() {
      CHECK(outcome.has_value(), base::NotFatalUntil::M136);
      if (outcome.has_value()) {
        base::UmaHistogramEnumeration("Discarding.DiscardPageOnUIThreadOutcome",
                                      outcome.value());
      }
    };

    if (!contents) {
      outcome = DiscardPageOnUIThreadOutcome::kNoContents;
      continue;
    }

    auto* lifecycle_unit = resource_coordinator::TabLifecycleUnitSource::
        GetTabLifecycleUnitExternal(contents.get());
    // This function is only called with `PageNode`s of type `kTab`, so there
    // should be a LifecycleUnit.
    CHECK(lifecycle_unit, base::NotFatalUntil::M136);
    if (!lifecycle_unit) {
      continue;
    }

    if (lifecycle_unit->DiscardTab(discard_reason, memory_footprint_estimate)) {
      outcome = DiscardPageOnUIThreadOutcome::kSuccess;
      discard_events.emplace_back(base::TimeTicks::Now(),
                                  memory_footprint_estimate);
    } else {
      outcome = DiscardPageOnUIThreadOutcome::kDiscardTabFailure;
    }
  }
  return discard_events;
}

}  // namespace

std::vector<PageDiscarder::DiscardEvent> PageDiscarder::DiscardPageNodes(
    const std::vector<const PageNode*>& page_nodes,
    resource_coordinator::LifecycleUnitDiscardReason discard_reason) {
  std::vector<WebContentsAndPmf> web_contents_and_pmf;
  web_contents_and_pmf.reserve(page_nodes.size());
  for (const auto* page_node : page_nodes) {
    web_contents_and_pmf.emplace_back(
        page_node->GetWebContents(),
        user_tuning::GetDiscardedMemoryEstimateForPage(page_node));
  }
  return DiscardPagesImpl(std::move(web_contents_and_pmf), discard_reason);
}

}  // namespace mechanism
}  // namespace performance_manager
