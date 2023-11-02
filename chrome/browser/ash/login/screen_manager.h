// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREEN_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREEN_MANAGER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "chrome/browser/ash/login/oobe_screen.h"

namespace ash {
class BaseScreen;

// Class that manages creation and ownership of screens.
class ScreenManager {
 public:
  ScreenManager();

  ScreenManager(const ScreenManager&) = delete;
  ScreenManager& operator=(const ScreenManager&) = delete;

  ~ScreenManager();

  // Initialize all screen instances.
  void Init(std::vector<std::pair<OobeScreenId, std::unique_ptr<BaseScreen>>>
                screens);

  // Destroys all screen instances.
  void Shutdown();

  // Getter for screen. Does not create the screen.
  BaseScreen* GetScreen(OobeScreenId screen);

  // Getter OobescreenId with both name and external_api_prefix
  // after fixing this https://crbug.com/1312879 .
  OobeScreenId GetScreenByName(const std::string& screen_name);

  bool HasScreen(OobeScreenId screen);

  void SetScreenForTesting(std::unique_ptr<BaseScreen> value);
  void DeleteScreenForTesting(OobeScreenId screen);

 private:
  // Created screens.
  base::flat_map<OobeScreenId, std::unique_ptr<BaseScreen>> screens_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREEN_MANAGER_H_
