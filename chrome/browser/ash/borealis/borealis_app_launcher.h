// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_APP_LAUNCHER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_APP_LAUNCHER_H_

#include <string>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"

namespace borealis {

class BorealisContext;

// Helper class responsible for launching borealis' apps.
class BorealisAppLauncher {
 public:
  enum class LaunchResult {
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

  // Launch the app with the given |app_id| and with the given |args| in the
  // borealis instance referred to by |ctx|.
  static void Launch(const BorealisContext& ctx,
                     const std::string& app_id,
                     const std::vector<std::string>& args,
                     OnLaunchedCallback callback);

  BorealisAppLauncher() = default;
  BorealisAppLauncher(const BorealisAppLauncher&) = delete;
  BorealisAppLauncher& operator=(const BorealisAppLauncher&) = delete;
  virtual ~BorealisAppLauncher() = default;

  // Launch the given |app_id|'s associated application. This can be the
  // borealis launcher itself or one of its GuestOsRegistry apps. |source|
  // indicates the source of the launch request.
  virtual void Launch(std::string app_id,
                      BorealisLaunchSource source,
                      OnLaunchedCallback callback) = 0;

  // Launch the given |app_id|'s associated application with the given |args|.
  // This can be the borealis launcher itself or one of its GuestOsRegistry
  // apps. |source| indicates the source of the launch request.
  virtual void Launch(std::string app_id,
                      const std::vector<std::string>& args,
                      BorealisLaunchSource source,
                      OnLaunchedCallback callback) = 0;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_APP_LAUNCHER_H_
