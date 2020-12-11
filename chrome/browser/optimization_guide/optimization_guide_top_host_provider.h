// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_TOP_HOST_PROVIDER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_TOP_HOST_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "components/optimization_guide/top_host_provider.h"

class PrefService;

namespace content {
class BrowserContext;
class NavigationHandle;
}  // namespace content

// An implementation of the optimization_guide::TopHostProvider for getting the
// top sites based on site engagement scores. This is the mechanism that has
// been approved for users that have Data Saver (aka Lite Mode) enabled.
class OptimizationGuideTopHostProvider
    : public optimization_guide::TopHostProvider {
 public:
  ~OptimizationGuideTopHostProvider() override;

  // Creates a OptimizationGuideTopHostProvider if the user is eligible to fetch
  // hints from the remote Optimization Guide Service.
  static std::unique_ptr<OptimizationGuideTopHostProvider> CreateIfAllowed(
      content::BrowserContext* browser_context);

  // Update the HintsFetcherTopHostBlacklist by attempting to remove the host
  // for the current navigation from the blacklist. A host is removed if it is
  // currently on the blacklist and the blacklist state is updated if the
  // blacklist is empty after removing a host.
  static void MaybeUpdateTopHostBlacklist(
      content::NavigationHandle* navigation_handle);

  // optimization_guide::TopHostProvider implementation:
  std::vector<std::string> GetTopHosts() override;

 private:
  OptimizationGuideTopHostProvider(content::BrowserContext* BrowserContext,
                                   base::Clock* time_clock);
  friend class OptimizationGuideTopHostProviderTest;

  // Initializes the HintsFetcherTopHostBlacklist with all the hosts in the site
  // engagement service and transitions the blacklist state from kNotInitialized
  // to kInitialized.
  void InitializeHintsFetcherTopHostBlacklist();

  // |browser_context_| is used for interaction with the SiteEngagementService
  // and the embedder should guarantee that it is non-null during the lifetime
  // of |this|.
  content::BrowserContext* browser_context_;

  // Clock used for getting current time.
  base::Clock* time_clock_;

  // |pref_service_| provides information about the current profile's
  // settings. It is not owned and guaranteed to outlive |this|.
  PrefService* pref_service_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideTopHostProvider);
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_TOP_HOST_PROVIDER_H_
