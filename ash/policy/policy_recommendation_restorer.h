// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_POLICY_POLICY_RECOMMENDATION_RESTORER_H_
#define ASH_POLICY_POLICY_RECOMMENDATION_RESTORER_H_

#include <memory>
#include <set>
#include <string>

#include "ash/public/cpp/session/session_observer.h"
#include "base/timer/timer.h"
#include "ui/base/user_activity/user_activity_observer.h"

class PrefChangeRegistrar;
class PrefService;

namespace ash {

// Manages observing a set of prefs on signin screen. If any of the prefs has a
// recommended value on observing, or changed to have recommended value or
// active user session started, its user settings is cleared so that the
// recommendation can take effect. On signin screen prefs, user settings are
// cleared when the user becomes idle for one minute.
//
// The above efforts are made to ensure that the observed prefs are *policy*
// overridden and can be restored properly. For example, a demo device on a
// store shelf. One customer walks up to device and enables some a11y features,
// leaving the device in a "funny" state (high contrast, screen magnifier,
// spoken feedback enabled). After some time, another customer won't feel the
// device looks "broken".
class PolicyRecommendationRestorer : public SessionObserver,
                                     public ui::UserActivityObserver {
 public:
  PolicyRecommendationRestorer();

  PolicyRecommendationRestorer(const PolicyRecommendationRestorer&) = delete;
  PolicyRecommendationRestorer& operator=(const PolicyRecommendationRestorer&) =
      delete;

  ~PolicyRecommendationRestorer() override;

  // Caller calls to start observing recommended value for |pref_name|. It
  // should be called when signin pref service is connected but before
  // observing/loading user settings for |pref_name|.
  void ObservePref(const std::string& pref_name);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // ui::UserActivityObserver:
  void OnUserActivity(const ui::Event* event) override;

  void DisableForTesting();

  base::OneShotTimer* restore_timer_for_test() { return &restore_timer_; }

 private:
  // If a recommended value and a user setting exist for |pref_name|, clears the
  // user setting so that the recommended value can take effect. If
  // |allow_delay| is |true| and user prefs not started yet, a timer is started
  // that will clear the setting when the user becomes idle for one minute.
  // Otherwise, the setting is cleared immediately.
  void Restore(bool allow_delay, const std::string& pref_name);

  void RestoreAll();

  void StartTimer();
  void StopTimer();

  std::set<std::string> pref_names_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  bool active_user_pref_connected_ = false;

  base::OneShotTimer restore_timer_;

  bool disabled_for_testing_ = false;
};

}  // namespace ash

#endif  // ASH_POLICY_POLICY_RECOMMENDATION_RESTORER_H_
