// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_FAST_PAIR_PREF_ENABLED_PROVIDER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_FAST_PAIR_PREF_ENABLED_PROVIDER_H_

#include "ash/public/cpp/session/session_observer.h"
#include "ash/quick_pair/feature_status_tracker/base_enabled_provider.h"
#include "ash/session/session_controller_impl.h"
#include "base/scoped_observation.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;

namespace ash {
namespace quick_pair {

// Observes whether a passed in boolean pref is enabled.
class FastPairPrefEnabledProvider : public BaseEnabledProvider,
                                    public SessionObserver {
 public:
  FastPairPrefEnabledProvider();
  ~FastPairPrefEnabledProvider() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

 private:
  void OnFastPairPrefChanged();

  // Observes user profile prefs.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::ScopedObservation<SessionControllerImpl, SessionObserver>
      session_observation_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_FAST_PAIR_PREF_ENABLED_PROVIDER_H_