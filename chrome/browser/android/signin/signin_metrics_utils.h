// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_METRICS_UTILS_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_METRICS_UTILS_H_

#include <string>
#include <vector>

namespace signin {
class IdentityManager;
}

namespace signin_metrics_utils {

// Logs web signin events. This method is called from java code when account
// picker bottom sheet is dismissed.
bool LogWebSignin(signin::IdentityManager* identity_manager,
                  const std::vector<std::string>& gaia_ids);

}  // namespace signin_metrics_utils

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_METRICS_UTILS_H_
