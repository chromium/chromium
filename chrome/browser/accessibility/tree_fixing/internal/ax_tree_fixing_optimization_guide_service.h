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
  // Delegate for clients that want to perform headings identification.
  class HeadingsIdentificationDelegate {
   protected:
    HeadingsIdentificationDelegate() = default;

   public:
    HeadingsIdentificationDelegate(const HeadingsIdentificationDelegate&) =
        delete;
    HeadingsIdentificationDelegate& operator=(
        const HeadingsIdentificationDelegate&) = delete;
    virtual ~HeadingsIdentificationDelegate() = default;

    // This method is used to communicate to the delegate (owner) of this
    // instance, the heading nodes that were identified via the IdentifyHeadings
    // method. When calling IdentifyHeadings, the client must provide a
    // request_id, and this ID is passed back to the client along with the
    // tree_id and headings. The request_id allows clients to make multiple
    // requests in parallel and uniquely identify each response. It is the
    // responsibility of the client to handle the logic behind a request_id,
    // this service simply passes the id through.
    virtual void OnHeadingsIdentified(const ui::AXTreeID& tree_id,
                                      const std::vector<ui::AXNodeID> headings,
                                      int request_id) = 0;
  };
  explicit AXTreeFixingOptimizationGuideService(
      HeadingsIdentificationDelegate& delegate,
      Profile* profile);
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

  // Delegate provided by client to receive headings identification results.
  // Use a raw_ref since we do not own the delegate or control its lifecycle.
  const raw_ref<HeadingsIdentificationDelegate>
      headings_identification_delegate_;

  // Profile for the KeyedService that owns us.
  const raw_ptr<Profile> profile_;

  base::WeakPtrFactory<AXTreeFixingOptimizationGuideService> weak_ptr_factory_{
      this};
};

}  // namespace tree_fixing

#endif  // CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_INTERNAL_AX_TREE_FIXING_OPTIMIZATION_GUIDE_SERVICE_H_
