// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PAI_STARTER_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PAI_STARTER_H_

#include <vector>

#include "ash/components/arc/mojom/app.mojom-forward.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"

class PrefService;
class Profile;

namespace arc {

// Helper class that starts Play Auto Install flow when all conditions are met:
// The Play Store app is ready and there is no lock for PAI flow.
class ArcPaiStarter : public ArcAppListPrefs::Observer {
 public:
  explicit ArcPaiStarter(Profile* profile);

  ArcPaiStarter(const ArcPaiStarter&) = delete;
  ArcPaiStarter& operator=(const ArcPaiStarter&) = delete;

  ~ArcPaiStarter() override;

  // Creates PAI starter in case it has not been executed for the requested
  // |context|.
  static std::unique_ptr<ArcPaiStarter> CreateIfNeeded(Profile* profile);

  // Registers callback that is called once PAI has been started. If PAI is
  // started already then callback is called immediately.
  void AddOnStartCallback(base::OnceClosure callback);

  // Triggers retry for testing. This fails in case retry is not scheduled.
  void TriggerRetryForTesting();

  // Returns true if PAI request was already started.
  bool started() const { return started_; }

 private:
  // Start PAI request if all conditions are met.
  //   * PAI is not yet started.
  //   * PAI is not locked externally.
  //   * PAI is not explicitly disabled in autotest.
  //   * Play Store app exists and is ready.
  //   * Request is not pending.
  void MaybeStartPai();
  // Called when PAI request completed successfully.
  void OnPaiDone();
  // Called when PAI request is done and |state| indicates state of the PAI flow
  // after the request.
  void OnPaiRequested(mojom::PaiFlowState state);

  // ArcAppListPrefs::Observer:
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;

  const raw_ptr<Profile> profile_;
  const raw_ptr<PrefService> pref_service_;
  std::vector<base::OnceClosure> onstart_callbacks_;
  // Set to true in case external component (Assistant) wants to lock PAI for
  // awhile.
  bool locked_ = false;
  // Set to true once PAI request was successfully processed.
  bool started_ = false;
  // Indicates that PAI request is pending.
  bool pending_ = false;
  // Used in case PAI request returns an error and we have to retry the request
  // after |retry_interval_seconds_|.
  base::OneShotTimer retry_timer_;
  // Contains interval for the next retry. Doubled on next attempt until reached
  // maximum value.
  base::TimeDelta retry_interval_;
  // Used to report PAI flow time uma.
  base::Time request_start_time_;
  // Keep last.
  base::WeakPtrFactory<ArcPaiStarter> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PAI_STARTER_H_
