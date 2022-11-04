// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_CONTROLLER_H_
#define CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_CONTROLLER_H_

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

class Adapter;
class AlsReader;
class BrightnessMonitorImpl;
class MetricsReporter;
class ModelConfigLoaderImpl;
class ModellerImpl;

// This controller class sets up and destroys all components needed for the auto
// screen brightness feature.
class Controller : public session_manager::SessionManagerObserver {
 public:
  Controller();

  Controller(const Controller&) = delete;
  Controller& operator=(const Controller&) = delete;

  ~Controller() override;

  // session_manager::SessionManagerObserver overrides:
  void OnUserSessionStarted(bool is_primary_user) override;

 private:
  // Initializes all components of the adaptive brightness system.
  // Controller will initialize the components once only, when the first user
  // signs in. By definition, the first sign-in user is the primary user, the
  // model will be personalized to this primary user.
  void InitializeComponents();

  // Whether all components of the adaptive brightness system are initialized.
  bool is_initialized_ = false;

  // Whether Controller is waiting for session manager's notification about
  // user sign-ins.
  bool observing_session_manager_ = false;

  std::unique_ptr<MetricsReporter> metrics_reporter_;
  std::unique_ptr<AlsReader> als_reader_;
  std::unique_ptr<BrightnessMonitorImpl> brightness_monitor_;
  std::unique_ptr<ModelConfigLoaderImpl> model_config_loader_;
  std::unique_ptr<ModellerImpl> modeller_;
  std::unique_ptr<Adapter> adapter_;
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_CONTROLLER_H_
