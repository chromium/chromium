// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_MAIN_MAC_H_
#define CHROME_APP_CHROME_MAIN_MAC_H_

namespace base {
class FilePath;
}

// Checks if the UserDataDir policy has been set and returns its value in the
// |user_data_dir| parameter. If no policy is set the parameter is not changed.
void CheckUserDataDirPolicy(base::FilePath* user_data_dir);

// Sets the app bundle (base::apple::FrameworkBundle()) to the framework's
// bundle, and sets the base bundle ID (base::apple::BaseBundleID()) to the
// proper value based on the running application. The base bundle ID is the
// outer browser application's bundle ID even when running in a non-browser
// (helper) process.
void SetUpBundleOverrides();

// Checks if the system or specific third-party applications launched a
// helper app directly in such a way that they cannot function.
bool IsHelperAppLaunchedBySystemOrThirdPartyApplication();

#endif  // CHROME_APP_CHROME_MAIN_MAC_H_
