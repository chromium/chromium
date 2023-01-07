// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_INSTALL_CHROME_APP_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_INSTALL_CHROME_APP_H_

#include <string>

namespace install_chrome_app {

// Initiates the UI flow for installing a Chrome app.
// Currently this is done by directing the user to the webstore page.
void InstallChromeApp(const std::string& app_id);

}  // namespace install_chrome_app

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_INSTALL_CHROME_APP_H_
