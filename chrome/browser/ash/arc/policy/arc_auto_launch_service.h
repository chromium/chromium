// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_POLICY_ARC_AUTO_LAUNCH_SERVICE_H_
#define CHROME_BROWSER_ASH_ARC_POLICY_ARC_AUTO_LAUNCH_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class ArcAppLauncher;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Manages auto-launching configured ARC applications on user login for
// managed users.
// Only the first app listed in the AutoLaunchAndroidApps policy is launched.
class ArcAutoLaunchService : public KeyedService {
 public:
  static ArcAutoLaunchService* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcAutoLaunchService* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcAutoLaunchService(content::BrowserContext* context,
                       ArcBridgeService* bridge_service);

  ArcAutoLaunchService(const ArcAutoLaunchService&) = delete;
  ArcAutoLaunchService& operator=(const ArcAutoLaunchService&) = delete;

  ~ArcAutoLaunchService() override;

  // KeyedService:
  void Shutdown() override;

  static void EnsureFactoryBuilt();

 private:
  // Starts launching `package_name`. Does nothing if it is empty.
  void StartLaunch(const std::string& package_name);
  // Launches the app if a launchable activity of `package_name_` is
  // registered.
  void MaybeLaunchApp();
  // Stops launching `package_name_`.
  void StopLaunch();

  const raw_ptr<Profile> profile_;

  // The package to launch. Empty if there is none.
  std::string package_name_;
  // Launches the app once it is ready.
  std::unique_ptr<ArcAppLauncher> app_launcher_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_POLICY_ARC_AUTO_LAUNCH_SERVICE_H_
