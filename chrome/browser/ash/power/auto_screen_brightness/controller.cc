// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/controller.h"

#include "base/time/default_tick_clock.h"
#include "chrome/browser/ash/power/auto_screen_brightness/adapter.h"
#include "chrome/browser/ash/power/auto_screen_brightness/als_reader.h"
#include "chrome/browser/ash/power/auto_screen_brightness/brightness_monitor_impl.h"
#include "chrome/browser/ash/power/auto_screen_brightness/gaussian_trainer.h"
#include "chrome/browser/ash/power/auto_screen_brightness/model_config_loader_impl.h"
#include "chrome/browser/ash/power/auto_screen_brightness/modeller_impl.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

Controller::Controller() {
  auto* session_manager = session_manager::SessionManager::Get();
  CHECK(session_manager, base::NotFatalUntil::M160);

  if (!session_manager->sessions().empty()) {
    // If a user session has been created, we can use the primary user's
    // browser context to initialize all components immediately without waiting
    // for |OnUserSessionStarted| to be called.
    InitializeComponents();
    return;
  }

  // Wait for a user session to be created.
  session_manager->AddObserver(this);
  observing_session_manager_ = true;
}

Controller::~Controller() {
  if (observing_session_manager_) {
    auto* session_manager = session_manager::SessionManager::Get();
    if (session_manager)
      session_manager->RemoveObserver(this);
  }
}

void Controller::OnUserSessionStarted(bool /* is_primary_user */) {
  // The first sign-in user is the primary user, hence if |OnUserSessionStarted|
  // is called, the primary user's browser context should have been created. We
  // will ignore |is_primary_user|.
  if (is_initialized_)
    return;

  InitializeComponents();
}

void Controller::InitializeComponents() {
  CHECK(!is_initialized_, base::NotFatalUntil::M160);
  is_initialized_ = true;

  const session_manager::Session* const primary_session =
      session_manager::SessionManager::Get()->GetPrimarySession();
  CHECK(primary_session, base::NotFatalUntil::M163);
  content::BrowserContext* const browser_context =
      BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          primary_session->account_id());
  CHECK(browser_context, base::NotFatalUntil::M163);

  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  CHECK(power_manager_client, base::NotFatalUntil::M160);

  als_reader_ = std::make_unique<AlsReader>();
  als_reader_->Init();

  brightness_monitor_ = std::make_unique<BrightnessMonitorImpl>();
  brightness_monitor_->Init();

  model_config_loader_ = std::make_unique<ModelConfigLoaderImpl>();

  ui::UserActivityDetector* user_activity_detector =
      ui::UserActivityDetector::Get();
  CHECK(user_activity_detector, base::NotFatalUntil::M160);

  modeller_ = std::make_unique<ModellerImpl>(
      browser_context->GetPath(), als_reader_.get(), brightness_monitor_.get(),
      model_config_loader_.get(), user_activity_detector,
      std::make_unique<GaussianTrainer>());

  adapter_ = std::make_unique<Adapter>(
      user_prefs::UserPrefs::Get(browser_context), als_reader_.get(),
      brightness_monitor_.get(), modeller_.get(), model_config_loader_.get());
  adapter_->Init();
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
