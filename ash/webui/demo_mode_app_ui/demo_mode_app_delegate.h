// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_APP_DELEGATE_H_
#define ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_APP_DELEGATE_H_

#include <string>

namespace ash {

// // Because //ash/webui can't depend on //chrome (plealse refer to
// //ash/webui/DEPS), we create a delegate class to supply //chrome/browser
// functionality to the Demo App code running in //ash/webui.
class DemoModeAppDelegate {
 public:
  virtual ~DemoModeAppDelegate() = default;

  // Launch an app by App Service app_id.
  virtual void LaunchApp(const std::string& app_id) = 0;

  // It calls DemoSession::RemoveSplashScreen() that lives in //chrome/browser
  // with the same name.
  virtual void RemoveSplashScreen() = 0;
};
}  // namespace ash
#endif  // ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_APP_DELEGATE_H_
