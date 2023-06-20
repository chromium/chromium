// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_VMM_ARC_SYSTEM_STATE_OBSERVATION_H_
#define CHROME_BROWSER_ASH_ARC_VMM_ARC_SYSTEM_STATE_OBSERVATION_H_

#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/throttle_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class BrowserContext;
}

namespace arc {

class PeaceDurationProvider {
 public:
  virtual ~PeaceDurationProvider() = default;
  virtual absl::optional<base::TimeDelta> GetPeaceDuration() = 0;
  virtual void SetDurationResetCallback(base::RepeatingClosure cb) = 0;
};

class ArcSystemStateObservation : public ash::ThrottleService,
                                  public ArcAppListPrefs::Observer,
                                  public PeaceDurationProvider {
 public:
  explicit ArcSystemStateObservation(content::BrowserContext* context);

  ArcSystemStateObservation(const ArcSystemStateObservation&) = delete;
  ArcSystemStateObservation& operator=(const ArcSystemStateObservation&) =
      delete;

  ~ArcSystemStateObservation() override;

  absl::optional<base::TimeDelta> GetPeaceDuration() override;

  void SetDurationResetCallback(base::RepeatingClosure cb) override;

  base::WeakPtr<ArcSystemStateObservation> GetWeakPtr();

  // ash::ThrottleService override:
  void ThrottleInstance(bool should_throttle) override;

  // ArcAppListPrefs::Observer:
  void OnAppStatesChanged(const std::string& id,
                          const ArcAppListPrefs::AppInfo& app_info) override;

  void OnArcAppListPrefsDestroyed() override;

 private:
  bool arc_running_ = false;

  absl::optional<base::Time> last_peace_timestamp_;
  base::RepeatingClosure active_callback_;

  base::ScopedObservation<ArcAppListPrefs, ArcAppListPrefs::Observer>
      app_prefs_observation_{this};
  base::WeakPtrFactory<ArcSystemStateObservation> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_VMM_ARC_SYSTEM_STATE_OBSERVATION_H_
