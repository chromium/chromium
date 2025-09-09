// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_PAGE_DISCARDER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_PAGE_DISCARDER_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/byte_count.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-forward.h"

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

  // Discards `page_node`. On success, returns the estimated amount of memory
  // freed. On failure, returns nullopt.
  virtual std::optional<base::ByteCount> DiscardPageNode(
      const PageNode* page_node,
      ::mojom::LifecycleUnitDiscardReason discard_reason);
};

}  // namespace mechanism
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_PAGE_DISCARDER_H_
