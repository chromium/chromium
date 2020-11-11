// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_INSTALL_UPDATER_H_
#define CHROME_BROWSER_MAC_INSTALL_UPDATER_H_

// Invokes the executable within the updater bundle at
// ../Versions/$(Version)/Helpers/Updater.app/Contents/Updater with the argument
// --install. That will copy the updater to its install directory and set up
// launchd plists. After the updater is installed, the IPC Registration API is
// invoked to register the browser to the updater.
void InstallUpdaterAndRegisterBrowser();

#endif  // CHROME_BROWSER_MAC_INSTALL_UPDATER_H_
