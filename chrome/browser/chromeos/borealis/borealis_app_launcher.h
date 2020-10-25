// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_APP_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_APP_LAUNCHER_H_

#include <string>

#include "base/bind_helpers.h"

namespace borealis {

class BorealisContext;

// Helper class responsible for launching borealis' apps.
class BorealisAppLauncher {
 public:
  enum LaunchResult {
    kSuccess,
    kUnknownApp,
    kNoResponse,
    kError,
  };
  using OnLaunchedCallback = base::OnceCallback<void(LaunchResult)>;
  // Launch the app with the given |app_id| in the borealis instance referred to
  // by |ctx|.
  static void Launch(const BorealisContext& ctx,
                     const std::string& app_id,
                     OnLaunchedCallback callback);
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_APP_LAUNCHER_H_
