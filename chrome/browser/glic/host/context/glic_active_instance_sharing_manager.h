// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_ACTIVE_BROWSER_SHARING_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_ACTIVE_BROWSER_SHARING_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/host/context/glic_delegating_sharing_manager.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"

class Profile;

namespace glic {
class GlicInstance;

class GlicInstanceCoordinator;

// Sharing manager that tracks with the active browser. When a Chrome window is
// active and its active tab is showing a GlicInstance, this sharing manager
// behaves like the sharing manager for that instance. Otherwise it behaves like
// an empty sharing manager (nothing is or can be shared).
class GlicActiveInstanceSharingManager : public GlicDelegatingSharingManager {
 public:
  explicit GlicActiveInstanceSharingManager(
      Profile* profile,
      GlicEnabling* enabling,
      GlicInstanceCoordinator* instance_coordinator);
  ~GlicActiveInstanceSharingManager() override;

  GlicActiveInstanceSharingManager(const GlicActiveInstanceSharingManager&) =
      delete;
  GlicActiveInstanceSharingManager& operator=(
      const GlicActiveInstanceSharingManager&) = delete;

 private:
  // Callback for changes to the last active GlicInstance.
  void OnActiveInstanceChanged(GlicInstance* instance);

  // Callback for changes to profile consent state.
  void OnProfileReadyStateChanged();

  // Helper to re-evaluate and set the correct delegate.
  void UpdateDelegate();

  // The profile this manager belongs to.
  const raw_ptr<Profile> profile_;

  raw_ptr<GlicInstanceCoordinator> instance_coordinator_;

  // Subscription for last active instance changes.
  base::CallbackListSubscription active_instance_subscription_;

  // Subscription for profile consent changes.
  base::CallbackListSubscription profile_state_subscription_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_ACTIVE_BROWSER_SHARING_MANAGER_H_
