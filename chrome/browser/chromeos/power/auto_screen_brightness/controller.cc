// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/controller.h"

#include "base/task/post_task.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/adapter.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/als_reader_impl.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/brightness_monitor_impl.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/gaussian_trainer.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/metrics_reporter.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/model_config_loader_impl.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/modeller_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

Controller::Controller() {
  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  DCHECK(power_manager_client);

  metrics_reporter_ = std::make_unique<MetricsReporter>(
      power_manager_client, g_browser_process->local_state());

  als_reader_ = std::make_unique<AlsReaderImpl>();
  als_reader_->Init();

  brightness_monitor_ = std::make_unique<BrightnessMonitorImpl>();
  brightness_monitor_->Init();

  model_config_loader_ = std::make_unique<ModelConfigLoaderImpl>();

  ui::UserActivityDetector* user_activity_detector =
      ui::UserActivityDetector::Get();
  DCHECK(user_activity_detector);

  Profile* const profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile);
  modeller_ = std::make_unique<ModellerImpl>(
      profile, als_reader_.get(), brightness_monitor_.get(),
      model_config_loader_.get(), user_activity_detector,
      std::make_unique<GaussianTrainer>());

  adapter_ = std::make_unique<Adapter>(
      profile, als_reader_.get(), brightness_monitor_.get(), modeller_.get(),
      model_config_loader_.get(), metrics_reporter_.get());
  adapter_->Init();
}

Controller::~Controller() = default;

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
