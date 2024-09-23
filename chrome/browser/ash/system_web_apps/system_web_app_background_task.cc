// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/system_web_app_background_task.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/idle/idle.h"

namespace ash {

SystemWebAppBackgroundTask::SystemWebAppBackgroundTask(
    Profile* profile,
    const SystemWebAppBackgroundTaskInfo& info)
    : profile_(profile),
      web_contents_(nullptr),
      web_app_url_loader_(std::make_unique<webapps::WebAppUrlLoader>()),
      timer_(std::make_unique<base::OneShotTimer>()),
      url_(info.url),
      period_(info.period),
      open_immediately_(info.open_immediately),
      delegate_(this) {}

SystemWebAppBackgroundTask::~SystemWebAppBackgroundTask() = default;

void SystemWebAppBackgroundTask::StartTask() {
  if (open_immediately_ ||
      period_ < base::Seconds(kInitialWaitForBackgroundTasksSeconds)) {
    timer_->Start(FROM_HERE,
                  base::Seconds(kInitialWaitForBackgroundTasksSeconds),
                  base::BindOnce(&SystemWebAppBackgroundTask::MaybeOpenPage,
                                 weak_ptr_factory_.GetWeakPtr()));
    state_ = INITIAL_WAIT;
  } else if (period_) {
    timer_->Start(FROM_HERE, period_.value(),
                  base::BindOnce(&SystemWebAppBackgroundTask::MaybeOpenPage,
                                 weak_ptr_factory_.GetWeakPtr()));
    state_ = WAIT_PERIOD;
  }
}

void SystemWebAppBackgroundTask::StopTask() {
  timer_.reset();
  web_contents_.reset();
}

void SystemWebAppBackgroundTask::MaybeOpenPage() {
  ui::IdleState idle_state = ui::CalculateIdleState(kIdleThresholdSeconds);
  base::Time now = base::Time::Now();
  // Start polling
  if (polling_since_time_.is_null()) {
    polling_since_time_ = now;
  }

  base::TimeDelta polling_duration = (now - polling_since_time_);

  if (polling_duration < base::Seconds(kIdlePollMaxTimeToWaitSeconds) &&
      idle_state == ui::IDLE_STATE_ACTIVE) {
    // We've gone through some weird clock adjustment (daylight savings?) that's
    // sent us back in time. We don't know what's going on, so zero the polling
    // time and stop polling.
    if (polling_duration < base::Seconds(0) && period_) {
      timer_->Start(FROM_HERE, period_.value(),
                    base::BindOnce(&SystemWebAppBackgroundTask::MaybeOpenPage,
                                   weak_ptr_factory_.GetWeakPtr()));
      state_ = WAIT_PERIOD;
      polling_since_time_ = base::Time();
      return;
    }
    // Poll
    timer_->Start(FROM_HERE, base::Seconds(kIdlePollIntervalSeconds),
                  base::BindOnce(&SystemWebAppBackgroundTask::MaybeOpenPage,
                                 weak_ptr_factory_.GetWeakPtr()));
    state_ = WAIT_IDLE;
    return;
  }

  if (period_) {
    timer_->Start(FROM_HERE, period_.value(),
                  base::BindOnce(&SystemWebAppBackgroundTask::MaybeOpenPage,
                                 weak_ptr_factory_.GetWeakPtr()));
  }

  polling_since_time_ = base::Time();
  state_ = WAIT_PERIOD;
  NavigateBackgroundPage();
}

void SystemWebAppBackgroundTask::CloseDelegate::CloseContents(
    content::WebContents* contents) {
  task_->CloseWebContents(contents);
}

void SystemWebAppBackgroundTask::CloseWebContents(
    content::WebContents* contents) {
  DCHECK(contents == web_contents_.get());
  web_contents_.reset();
}

void SystemWebAppBackgroundTask::NavigateBackgroundPage() {
  if (!web_contents_) {
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_contents_->SetDelegate(&delegate_);
  }

  timer_activated_count_++;
  auto prefs = web_contents_->GetOrCreateWebPreferences();

  prefs.allow_scripts_to_close_windows = true;
  web_contents_->SetWebPreferences(prefs);
  web_app_url_loader_->LoadUrl(
      url_, web_contents_.get(),
      webapps::WebAppUrlLoader::UrlComparison::kExact,
      base::BindOnce(&SystemWebAppBackgroundTask::OnPageReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SystemWebAppBackgroundTask::OnPageReady(
    webapps::WebAppUrlLoaderResult result) {
  if (result == webapps::WebAppUrlLoaderResult::kUrlLoaded) {
    opened_count_++;
  }
}

}  // namespace ash
