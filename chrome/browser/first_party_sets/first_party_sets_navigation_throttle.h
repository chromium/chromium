// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

class Profile;

namespace first_party_sets {

// Observes navigations and defers navigations of outermost frames on
// First-Party Sets initialization during startup.
class FirstPartySetsNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit FirstPartySetsNavigationThrottle(content::NavigationHandle* handle);
  ~FirstPartySetsNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;

  // Only create throttle if FPS clearing is enabled and this is the outermost
  // frame navigation; returns nullptr otherwise.
  static std::unique_ptr<FirstPartySetsNavigationThrottle>
  MaybeCreateNavigationThrottle(content::NavigationHandle* navigation_handle);

 private:
  raw_ref<Profile> profile_;

  base::WeakPtrFactory<FirstPartySetsNavigationThrottle> weak_factory_{this};
};

}  // namespace first_party_sets

#endif  // CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_NAVIGATION_THROTTLE_H_
