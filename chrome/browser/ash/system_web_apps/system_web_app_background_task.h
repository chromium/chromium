// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_BACKGROUND_TASK_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_BACKGROUND_TASK_H_

#include <memory.h>

#include <optional>
#include <utility>

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_background_task_info.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

class Profile;

namespace ash {

// Used to manage a running periodic background task for a SWA.
class SystemWebAppBackgroundTask {
 public:
  enum TimerState {
    INITIAL_WAIT = 0,
    WAIT_PERIOD = 1,
    WAIT_IDLE = 2,
    INACTIVE = 3
  };

  // Wait for 2 minutes before starting background tasks. User login is busy,
  // and this will give a little time to settle down. We could get even more
  // sophisticated, and smear all the different start_immediately tasks across a
  // couple minutes instead of setting their start timers to the same time.
  static const int kInitialWaitForBackgroundTasksSeconds = 120;

  // User idle for 1 minute.
  static const int kIdleThresholdSeconds = 60;

  // Else, poll every 30 seconds
  static const int kIdlePollIntervalSeconds = 30;

  // For up to an hour.
  static const int kIdlePollMaxTimeToWaitSeconds = 3600;

  SystemWebAppBackgroundTask(Profile* profile,
                             const SystemWebAppBackgroundTaskInfo& info);
  ~SystemWebAppBackgroundTask();

  // Start the timer, at the specified period. This will also run immediately if
  // needed
  void StartTask();

  // Bring down the background task if open, and stop the timer.
  void StopTask();

  bool open_immediately_for_testing() const { return open_immediately_; }

  SystemWebAppType app_type_for_testing() const { return app_type_; }

  GURL url_for_testing() const { return url_; }

  content::WebContents* web_contents_for_testing() const {
    return web_contents_.get();
  }

  std::optional<base::TimeDelta> period_for_testing() const { return period_; }

  unsigned long opened_count_for_testing() const { return opened_count_; }

  unsigned long timer_activated_count_for_testing() const {
    return timer_activated_count_;
  }

  base::Time polling_since_time_for_testing() const {
    return polling_since_time_;
  }

  webapps::WebAppUrlLoader* UrlLoaderForTesting() {
    return web_app_url_loader_.get();
  }

  // Set the url loader for testing. Takes ownership of the argument.
  void SetUrlLoaderForTesting(
      std::unique_ptr<webapps::WebAppUrlLoader> loader) {
    web_app_url_loader_ = std::move(loader);
  }

  TimerState get_state_for_testing() const { return state_; }

  base::OneShotTimer* get_timer_for_testing() { return timer_.get(); }

 private:
  // A delegate to reset the WebContents owned by this background task and free
  // up the resources. Called when the page calls window.close() to exit.
  class CloseDelegate : public content::WebContentsDelegate {
   public:
    explicit CloseDelegate(SystemWebAppBackgroundTask* task) : task_(task) {}
    void CloseContents(content::WebContents* contents) override;

   private:
    raw_ptr<SystemWebAppBackgroundTask> task_;
  };
  // A state machine to either poll and fail, stop polling and succeed, or stop
  // polling and fail
  void MaybeOpenPage();

  void NavigateBackgroundPage();
  void OnPageReady(webapps::WebAppUrlLoaderResult);

  void CloseWebContents(content::WebContents* contents);

  raw_ptr<Profile> profile_;
  SystemWebAppType app_type_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<webapps::WebAppUrlLoader> web_app_url_loader_;
  std::unique_ptr<base::OneShotTimer> timer_;
  TimerState state_;
  GURL url_;
  std::optional<base::TimeDelta> period_;
  unsigned long opened_count_ = 0U;
  unsigned long timer_activated_count_ = 0U;
  bool open_immediately_ = false;
  base::Time polling_since_time_;
  CloseDelegate delegate_;

  base::WeakPtrFactory<SystemWebAppBackgroundTask> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_BACKGROUND_TASK_H_
