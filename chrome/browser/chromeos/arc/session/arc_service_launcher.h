// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_SESSION_ARC_SERVICE_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_SESSION_ARC_SERVICE_LAUNCHER_H_

#include <memory>

#include "ash/public/cpp/default_scale_factor_retriever.h"
#include "base/macros.h"

class Profile;

namespace ash {
class DefaultScaleFactorRetriever;
}

namespace chromeos {
class SchedulerConfigurationManagerBase;
}

namespace arc {

class ArcPlayStoreEnabledPreferenceHandler;
class ArcServiceManager;
class ArcSessionManager;

// Detects ARC availability and launches ARC bridge service.
class ArcServiceLauncher {
 public:
  // |scheduler_configuration_manager| must outlive |this| object.
  explicit ArcServiceLauncher(chromeos::SchedulerConfigurationManagerBase*
                                  scheduler_configuration_manager);
  ~ArcServiceLauncher();

  // Returns a global instance.
  static ArcServiceLauncher* Get();

  // Called just before most of BrowserContextKeyedService instance creation.
  // Set the given |profile| to ArcSessionManager, if the profile is allowed
  // to use ARC.
  void MaybeSetProfile(Profile* profile);

  // Called when the main profile is initialized after user logs in.
  void OnPrimaryUserProfilePrepared(Profile* profile);

  // Called after the main MessageLoop stops, and before the Profile is
  // destroyed.
  void Shutdown();

  // Resets internal state for testing. Specifically this needs to be
  // called if other profile needs to be used in the tests. In that case,
  // following this call, MaybeSetProfile() and
  // OnPrimaryUserProfilePrepared() should be called.
  void ResetForTesting();

 private:
  ash::DefaultScaleFactorRetriever default_scale_factor_retriever_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<ArcPlayStoreEnabledPreferenceHandler>
      arc_play_store_enabled_preference_handler_;
  // |scheduler_configuration_manager_| outlives |this|.
  chromeos::SchedulerConfigurationManagerBase* const
      scheduler_configuration_manager_;

  DISALLOW_COPY_AND_ASSIGN(ArcServiceLauncher);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_SESSION_ARC_SERVICE_LAUNCHER_H_
