// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLATFORM_EXPERIENCE_INSTALLER_INSTALLER_WIN_H_
#define CHROME_BROWSER_PLATFORM_EXPERIENCE_INSTALLER_INSTALLER_WIN_H_

namespace platform_experience {

// Starts the installation of the PEH, if it hasn't already been installed yet.
// This function might block.
void MaybeInstallPlatformExperienceHelper();

}  // namespace platform_experience

#endif  // CHROME_BROWSER_PLATFORM_EXPERIENCE_INSTALLER_INSTALLER_WIN_H_
