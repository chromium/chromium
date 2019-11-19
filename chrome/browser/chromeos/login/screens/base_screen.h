// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "components/login/base_screen_handler_utils.h"

namespace chromeos {

// Base class for the all OOBE/login/before-session screens.
// Screens are identified by ID, screen and it's JS counterpart must have same
// id.
// Most of the screens will be re-created for each appearance with Initialize()
// method called just once.
class BaseScreen {
 public:
  explicit BaseScreen(OobeScreenId screen_id);
  virtual ~BaseScreen();

  // Makes wizard screen visible.
  virtual void Show() = 0;

  // Makes wizard screen invisible.
  virtual void Hide() = 0;

  // Returns the identifier of the screen.
  OobeScreenId screen_id() const { return screen_id_; }

  // Called when user action event with |event_id|
  // happened. Notification about this event comes from the JS
  // counterpart.
  virtual void OnUserAction(const std::string& action_id);

  // Change the configuration for the screen. |configuration| is unowned.
  virtual void SetConfiguration(base::Value* configuration);

 protected:
  // Global configuration for OOBE screens, that can be used to automate some
  // screens.
  // Screens can use values in Configuration to fill in UI values or
  // automatically finish.
  // Configuration is guaranteed to exist between pair of OnShow/OnHide calls,
  // no external changes will be made to configuration during that time.
  // Outside that time the configuration is set to nullptr to prevent any logic
  // triggering while the screen is not displayed.
  base::Value* GetConfiguration() { return configuration_; }

 private:
  // Configuration itself is owned by WizardController and is accessible
  // to screen only between OnShow / OnHide calls.
  base::Value* configuration_ = nullptr;

  const OobeScreenId screen_id_;

  DISALLOW_COPY_AND_ASSIGN(BaseScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_H_
