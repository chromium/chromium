// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CHROME_DEMO_MODE_APP_DELEGATE_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CHROME_DEMO_MODE_APP_DELEGATE_H_

#include "ash/webui/demo_mode_app_ui/demo_mode_app_delegate.h"
#include "content/public/browser/web_ui.h"

namespace ash {

// Implementation of the DemoModeAppDelegate interface that provides //chrome
// functionality to the Demo Mode App living in //ash.
class ChromeDemoModeAppDelegate : public DemoModeAppDelegate {
 public:
  explicit ChromeDemoModeAppDelegate(content::WebUI* web_ui);
  ChromeDemoModeAppDelegate(const ChromeDemoModeAppDelegate&) = delete;
  ChromeDemoModeAppDelegate& operator=(const ChromeDemoModeAppDelegate&) =
      delete;
  ~ChromeDemoModeAppDelegate() override = default;

  // DemoModeAppDelegate
  void LaunchApp(const std::string& app_id) override;

  // DemoModeAppDelegate
  void RemoveSplashScreen() override;

 private:
  raw_ptr<content::WebUI> web_ui_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CHROME_DEMO_MODE_APP_DELEGATE_H_
