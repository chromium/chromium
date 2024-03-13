// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_ARC_UPGRADE_PARAMS_H_
#define ASH_COMPONENTS_ARC_SESSION_ARC_UPGRADE_PARAMS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ash/components/arc/session/arc_management_transition.h"
#include "base/files/file_path.h"

namespace arc {

constexpr char kPackagesCacheModeCopy[] = "copy";
constexpr char kPackagesCacheModeSkipCopy[] = "skip-copy";

// Parameters to upgrade request.
struct UpgradeParams {
  enum class PackageCacheMode {
    // Performs packages cache setup if the pre-generated cache exists.
    DEFAULT = 0,
    // Performs packages cache setup if the pre-generated cache exists and
    // copies resulting packages.xml to the temporary location after
    // SystemServer initialized the package manager.
    COPY_ON_INIT,
    // Skips packages cache setup and copies resulting packages.xml to the
    // temporary location after SystemServer initialized the package manager.
    SKIP_SETUP_COPY_ON_INIT,
  };

  // Explicit ctor/dtor declaration is necessary for complex struct. See
  // https://cs.chromium.org/chromium/src/tools/clang/plugins/FindBadConstructsConsumer.cpp
  UpgradeParams();
  ~UpgradeParams();
  // Intentionally allows copying. The parameter is for container restart.
  UpgradeParams(const UpgradeParams& other);
  UpgradeParams(UpgradeParams&& other);
  UpgradeParams& operator=(UpgradeParams&& other);

  // Account ID of the user to start ARC for.
  std::string account_id;

  // Whether the account is managed.
  bool is_account_managed;

  // Whether adb sideloading is allowed when the account and/or the device is
  // managed.
  bool is_managed_adb_sideloading_allowed = false;

  // Whether adb sideloading is enabled or not.
  // This parameter is used only for ARCVM.
  bool is_adb_sideloading_enabled = false;

  // Option to disable ACTION_BOOT_COMPLETED broadcast for 3rd party apps.
  // The constructor automatically populates this from command-line.
  bool skip_boot_completed_broadcast;

  // Optional mode for packages cache tests.
  // The constructor automatically populates this from command-line.
  PackageCacheMode packages_cache_mode;

  // Option to disable GMS CORE cache.
  // The constructor automatically populates this from command-line.
  bool skip_gms_core_cache;

  // Option to disable TTS cache.
  // The constructor automatically populates this from command-line.
  bool skip_tts_cache;

  // Option to disable DexOpt cache.
  // The constructor automatically populates this from command-line.
  bool skip_dexopt_cache;

  // The supervision transition state for this account. Indicates whether
  // child account should become regular, regular account should become child
  // or neither.
  ArcManagementTransition management_transition =
      ArcManagementTransition::NO_TRANSITION;

  // Define language configuration set during Android container boot.
  // |preferred_languages| may be empty.
  std::string locale;
  std::vector<std::string> preferred_languages;

  // Whether ARC is being upgraded in a demo session.
  bool is_demo_session = false;

  // |demo_session_apps_path| is a file path to the image containing set of
  // demo apps that should be pre-installed into the Android container for
  // demo sessions. It might be empty, in which case no demo apps will be
  // pre-installed.
  // Should be empty if |is_demo_session| is not set.
  base::FilePath demo_session_apps_path;

  // Flag to enable ARC Nearby Share support.
  bool enable_arc_nearby_share = true;

  // Flag to enable a delay for killing high priority app under memory pressure.
  bool enable_priority_app_lmk_delay = false;

  // Delay time in second until a high priority app can be considered to be
  // killed.
  uint32_t priority_app_lmk_delay_second = 0;

  // Comma separated list of high priority apps that would have a delay before
  // considered to be killed.
  std::string priority_app_lmk_delay_list;

  // Flag to enable update for minimum Android process state to be considered to
  // be killed under perceptible memory pressure
  bool enable_lmk_perceptible_min_state_update = false;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_ARC_UPGRADE_PARAMS_H_
