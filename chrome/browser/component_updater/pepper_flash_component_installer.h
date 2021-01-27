// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PEPPER_FLASH_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PEPPER_FLASH_COMPONENT_INSTALLER_H_

namespace base {
class FilePath;
}

namespace component_updater {

// Deletes any Flash component implementations that still reside on disk.
// Historically, Flash was delivered via component update. It has since been
// removed, but this function still is called to clean up any existing
// flash component files.
void CleanUpPepperFlashComponent(const base::FilePath& profile_path);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PEPPER_FLASH_COMPONENT_INSTALLER_H_
