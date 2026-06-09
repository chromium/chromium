// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_ACTIVE_INSTANCE_SHARING_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_ACTIVE_INSTANCE_SHARING_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/host/context/glic_delegating_sharing_manager.h"

class Profile;

namespace glic {
class GlicEnabling;

// Sharing manager that tracks the active instance's sharing manager,
// while also respecting profile consent state.
// Owned by GlicInstanceCoordinatorImpl.
class GlicActiveInstanceSharingManager : public GlicDelegatingSharingManager {
 public:
  GlicActiveInstanceSharingManager(Profile* profile, GlicEnabling* enabling);
  ~GlicActiveInstanceSharingManager() override;

  GlicActiveInstanceSharingManager(const GlicActiveInstanceSharingManager&) =
      delete;
  GlicActiveInstanceSharingManager& operator=(
      const GlicActiveInstanceSharingManager&) = delete;

  // Sets the sharing manager of the currently active instance. Can be null.
  // The caller must ensure this is changed (or set to null) before the
  // previously active sharing manager is destroyed.
  void SetActiveSharingManager(GlicSharingManagerInternal* sharing_manager);

 private:
  // Callback for changes to profile consent state.
  void OnProfileReadyStateChanged();

  // Helper to re-evaluate and set the correct delegate.
  void UpdateDelegate();

  // The profile this manager belongs to.
  const raw_ptr<Profile> profile_;

  // The sharing manager of the currently active instance.
  raw_ptr<GlicSharingManagerInternal> active_sharing_manager_ = nullptr;

  // Subscription for profile consent changes.
  base::CallbackListSubscription profile_state_subscription_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_ACTIVE_INSTANCE_SHARING_MANAGER_H_
