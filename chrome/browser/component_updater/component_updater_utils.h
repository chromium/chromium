// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_UTILS_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_UTILS_H_

namespace component_updater {

// Returns true if Chrome is installed for the current user, or false
// if Chrome is installed for all users on the machine. Some platforms
// don't support this type of install. In this case, assume it is a user
// install and return true.
bool IsPerUserInstall();

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_UTILS_H_
