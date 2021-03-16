// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_APP_LAUNCHER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_APP_LAUNCHER_H_

#include <string>

#include "base/callback_helpers.h"

class Profile;

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

  explicit BorealisAppLauncher(Profile* profile);

  // Launch the given |app_id|'s associated application. This can be the
  // borealis launcher itself or one of its GuestOsRegistry apps.
  void Launch(std::string app_id, OnLaunchedCallback callback);

  // Launch the given |app_id|'s associated application with the given |args|.
  // This can be the borealis launcher itself or one of its GuestOsRegistry
  // apps.
  void Launch(std::string app_id,
              const std::vector<std::string>& args,
              OnLaunchedCallback callback);

 private:
  Profile* const profile_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_APP_LAUNCHER_H_
