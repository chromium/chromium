// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_INTERNAL_AX_TREE_FIXING_OPTIMIZATION_GUIDE_SERVICE_H_
#define CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_INTERNAL_AX_TREE_FIXING_OPTIMIZATION_GUIDE_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_tree_update.h"

namespace tree_fixing {

// This class provides a connection to the Optimization Guide service. This
// service should only be constructed by clients when the client requires the
// Optimization Guide service (and not earlier). The client that constructs this
// service owns the object and needs to handle tear-down.
class AXTreeFixingOptimizationGuideService final {
 public:
  explicit AXTreeFixingOptimizationGuideService(Profile* profile);
  AXTreeFixingOptimizationGuideService(
      const AXTreeFixingOptimizationGuideService&) = delete;
  AXTreeFixingOptimizationGuideService& operator=(
      const AXTreeFixingOptimizationGuideService&) = delete;
  ~AXTreeFixingOptimizationGuideService();

  // --- Public APIs for upstream clients (e.g. AXTreeFixingServicesRouter) ---

  // Identifies the headings of a page given an AXTreeUpdate and a screenshot.
  // The client should provide a request_id, which is returned to the client.
  void IdentifyHeadings(const ui::AXTreeUpdate& ax_tree_update,
                        const SkBitmap& bitmap,
                        int request_id);

 private:
  // Internal methods related to managing Optimization Guide service connection.
  void Initialize();

  // Profile for the KeyedService that owns us.
  const raw_ptr<Profile> profile_;

  base::WeakPtrFactory<AXTreeFixingOptimizationGuideService> weak_ptr_factory_{
      this};
};

}  // namespace tree_fixing

#endif  // CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_INTERNAL_AX_TREE_FIXING_OPTIMIZATION_GUIDE_SERVICE_H_
