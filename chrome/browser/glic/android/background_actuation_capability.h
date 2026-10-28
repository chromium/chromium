// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_ANDROID_BACKGROUND_ACTUATION_CAPABILITY_H_
#define CHROME_BROWSER_GLIC_ANDROID_BACKGROUND_ACTUATION_CAPABILITY_H_

#include <memory>
#include <optional>

#include "base/android/application_status_listener.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class PrefService;

namespace glic {

// Caches whether this device can run a Glic experimental triggering task in
// the background (`CanActuateInBackground()`), which controls the
// "background_actuation" capability. Reading it needs JNI and binder calls, so
// it starts from the last known value in
// `prefs::kGlicLastCanActuateInBackground` and is re-read off the startup path:
// after construction, when Chrome returns to the foreground, and on sign-in.
// It's never read while the profile is signed out. `GlicEnabling` only creates
// it when `kGlicBackgroundActuation` is enabled.
class BackgroundActuationCapability : public signin::IdentityManager::Observer {
 public:
  // `prefs` and `identity_manager` are retained and must outlive this object.
  // If either is null, this does nothing and `can_actuate_in_background()`
  // stays false.
  BackgroundActuationCapability(PrefService* prefs,
                                signin::IdentityManager* identity_manager,
                                base::RepeatingClosure on_changed);
  BackgroundActuationCapability(const BackgroundActuationCapability&) = delete;
  BackgroundActuationCapability& operator=(
      const BackgroundActuationCapability&) = delete;
  ~BackgroundActuationCapability() override;

  bool can_actuate_in_background() const { return can_actuate_in_background_; }

  void RefreshForTesting();

  // Overrides `CanActuateInBackground()`; `std::nullopt` clears it.
  static void SetCanActuateInBackgroundForTesting(std::optional<bool> can);

 private:
  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  // Re-reads and persists the value, and runs `on_changed` if it changed.
  void Refresh();
  void OnApplicationStateChanged(base::android::ApplicationState state);
  void ScheduleRefresh();
  void OnRefreshTask();

  const raw_ptr<PrefService> prefs_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  base::RepeatingClosure on_changed_;
  bool can_actuate_in_background_ = false;
  bool refresh_pending_ = false;
  std::unique_ptr<base::android::ApplicationStatusListener>
      app_status_listener_;
  base::WeakPtrFactory<BackgroundActuationCapability> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ANDROID_BACKGROUND_ACTUATION_CAPABILITY_H_
