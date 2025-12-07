// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_IDLE_HANDLER_H_
#define CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_IDLE_HANDLER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_window_closer.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/files_cleanup_handler.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace ash {

class IdleDetector;

// Watch user activity and handle actions for idle for Demo mode. When device is
// idle, reset device states(i.e. close all windows, restart attract loop...)
// and wait for next user.
class DemoModeIdleHandler : public ui::UserActivityObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called on local files is cleaned up complete.
    virtual void OnLocalFilesCleanupCompleted() = 0;
  };

  DemoModeIdleHandler(
      DemoModeWindowCloser* window_closer,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);
  DemoModeIdleHandler(const DemoModeIdleHandler&) = delete;
  DemoModeIdleHandler& operator=(const DemoModeIdleHandler&) = delete;
  ~DemoModeIdleHandler() override;

  // ui::UserActivityObserver:
  void OnUserActivity(const ui::Event* event) override;

  // Adds an observer to the observer list.
  void AddObserver(Observer* observer);

  // Removes an observer from the observer list.
  void RemoveObserver(Observer* observer);

  const std::optional<base::OneShotTimer>& GetMGSLogoutTimeoutForTest() const {
    return mgs_logout_timer_;
  }
  void SetIdleTimeoutForTest(std::optional<base::TimeDelta> timeout);

 private:
  // Called on idle timeout reaches. Could be invoked on non-UI thread when
  // using `base::TestMockTimeTaskRunner` for test.
  void OnIdle();

  // Cleans up everything under "MyFiles" and reset "Downloads" folder to
  // empty.
  void CleanupLocalFiles();
  // Populates demo files after cleanup.
  void OnLocalFilesCleanupCompleted(
      const std::optional<std::string>& error_message);

  // True when the device is not idle.
  bool is_user_active_ = false;

  // Detect idle when attract loop is not playing. If the attract loop is well
  // function and it is not playing, it indicates that a user is actively engage
  // with device.
  std::unique_ptr<IdleDetector> idle_detector_;

  // Timer to schedule logout for the fallback MGS due to peak time requests of
  // demo account.
  std::optional<base::OneShotTimer> mgs_logout_timer_;

  // Not owned:
  raw_ptr<DemoModeWindowCloser> window_closer_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // Cleaner for `MyFiles` directory:
  chromeos::FilesCleanupHandler file_cleaner_;

  std::optional<base::TimeDelta> idle_time_out_for_test_;

  base::ScopedObservation<ui::UserActivityDetector, ui::UserActivityObserver>
      user_activity_observer_{this};

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<DemoModeIdleHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_IDLE_HANDLER_H_
