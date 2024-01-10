// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_PAGE_DISCARDER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_PAGE_DISCARDER_H_

#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"

namespace performance_manager {

class PageNode;

namespace mechanism {

// Mechanism that allows discarding a PageNode.
class PageDiscarder {
 public:
  PageDiscarder() = default;
  virtual ~PageDiscarder() = default;
  PageDiscarder(const PageDiscarder& other) = delete;
  PageDiscarder& operator=(const PageDiscarder&) = delete;

  // When invoked, DiscardPageNodes() becomes a no-op.
  static void DisableForTesting();

  struct DiscardEvent {
    base::TimeTicks discard_time;
    uint64_t estimated_memory_freed_kb = 0;
  };

  // Discards |page_nodes| and runs |post_discard_cb| on the origin sequence
  // once this is done.
  virtual void DiscardPageNodes(
      const std::vector<const PageNode*>& page_nodes,
      ::mojom::LifecycleUnitDiscardReason discard_reason,
      base::OnceCallback<void(const std::vector<DiscardEvent>&)>
          post_discard_cb);
};

}  // namespace mechanism
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_PAGE_DISCARDER_H_
