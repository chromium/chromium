// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_BLINK_BLINK_OPTIMIZATION_GUIDE_INQUIRER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_BLINK_BLINK_OPTIMIZATION_GUIDE_INQUIRER_H_

#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "third_party/blink/public/mojom/optimization_guide/optimization_guide.mojom.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace optimization_guide {

// BlinkOptimizationGuideInquirer asks the optimization guide service about
// hints for optimization types for Blink. This is instantiated per main frame
// by BlinkOptimizationGuideWebContentsObserver, and destroyed when the next
// main frame navigation gets ready to commit.
class BlinkOptimizationGuideInquirer {
 public:
  // Creates an instance of this class, and starts an inquiry.
  static std::unique_ptr<BlinkOptimizationGuideInquirer> CreateAndStart(
      content::NavigationHandle& navigation_handle,
      OptimizationGuideDecider& decider);

  ~BlinkOptimizationGuideInquirer();

  BlinkOptimizationGuideInquirer(const BlinkOptimizationGuideInquirer&) =
      delete;
  BlinkOptimizationGuideInquirer& operator=(
      const BlinkOptimizationGuideInquirer&) = delete;
  BlinkOptimizationGuideInquirer(BlinkOptimizationGuideInquirer&&) = delete;
  BlinkOptimizationGuideInquirer& operator=(BlinkOptimizationGuideInquirer&&) =
      delete;

  // Returns a snapshot of the hints currently available. This may not contain
  // information for some optimization types yet because the optimization guide
  // service needs network access to provide the hints when they are not locally
  // cached.
  blink::mojom::BlinkOptimizationGuideHintsPtr GetHints() {
    return optimization_guide_hints_.Clone();
  }

 private:
  BlinkOptimizationGuideInquirer();

  // Asks the optimization guide service about the hints. When the hints are
  // locally cached in the service, this synchronously updates
  // |optimization_guide_hints_|.
  void InquireHints(content::NavigationHandle& navigation_handle,
                    OptimizationGuideDecider& decider);
  void DidInquireHints(proto::OptimizationType optimization_type,
                       OptimizationGuideDecision decision,
                       const OptimizationMetadata& metadata);

  void PopulateHintsForDelayAsyncScriptExecution(
      const OptimizationMetadata& optimization_metadata);
  void PopulateHintsForDelayCompetingLowPriorityRequests(
      const OptimizationMetadata& optimization_metadata);

  // The hints currently available.
  blink::mojom::BlinkOptimizationGuideHintsPtr optimization_guide_hints_;

  base::WeakPtrFactory<BlinkOptimizationGuideInquirer> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_BLINK_BLINK_OPTIMIZATION_GUIDE_INQUIRER_H_
