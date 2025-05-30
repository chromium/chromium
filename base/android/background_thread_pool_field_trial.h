// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_BACKGROUND_THREAD_POOL_FIELD_TRIAL_H_
#define BASE_ANDROID_BACKGROUND_THREAD_POOL_FIELD_TRIAL_H_

#include <optional>
#include <string_view>

#include "base/auto_reset.h"
#include "base/base_export.h"
#include "base/synchronization/synchronization_buildflags.h"

namespace base::android {

class BASE_EXPORT BackgroundThreadPoolFieldTrial {
 public:
  static void Initialize();

  // Returns true if the background thread pool field trial is active and has
  // requested the use of priority-inheritance locks.
  static bool ShouldUsePriorityInheritanceLocks();

  // Returns true if the background thread pool field trial is active and has
  // requested the use of background thread pool.
  static bool ShouldUseBackgroundThreadPool();

  struct BASE_EXPORT TrialInfo {
    std::string trial_name;
    std::string group_name;
  };

  // Returns a non-null TrialInfo struct if the trial is active.
  static std::optional<TrialInfo> GetTrialInfo();

 protected:
  // The configuration value dictates the trial and group currently active. It
  // is
  // set by the server-side config and applied on the next browser launch.
  enum class Configuration : int {
    // |kDisabled|: No trial is currently active.
    kDisabled = 0,

    // The PI supported trial groups requires the kernel to support priority
    // inheritance futexes. It is intended
    // to measure the effect of PI-locks along with the background thread pool.
    kPISupportedTrialControl = 1,
    kPISupportedTrialEnabledPILocksOnly = 2,
    kPISupportedTrialEnabledBGThreadPoolOnly = 3,
    kPISupportedTrialEnabledBoth = 4,

    // The general trial does not require any special kernel support and is
    // meant
    // to measure the effect of
    // using a background thread pool unconditionally.
    kGeneralTrialControl = 5,
    kGeneralTrialEnabledBGThreadPool = 6,

    // |kConfigurationMax|: The maximum integer value of the configuration.
    kConfigurationMax = kGeneralTrialEnabledBGThreadPool,
  };

  friend class ScopedUsePriorityInheritanceLocksForTesting;

  static std::optional<Configuration> s_configuration_;

 private:
  static Configuration ReadConfigurationFromCommandLine();
  static Configuration GetConfiguration();
  static TrialInfo GetGeneralTrialInfo();
#if BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)
  static TrialInfo GetPISupportedTrialInfo();
#endif  // BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)
};

#if BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)
class BASE_EXPORT ScopedUsePriorityInheritanceLocksForTesting {
 public:
  ScopedUsePriorityInheritanceLocksForTesting();
  ~ScopedUsePriorityInheritanceLocksForTesting();

 private:
  base::AutoReset<std::optional<BackgroundThreadPoolFieldTrial::Configuration>>
      reset_config_value_;
};
#endif  // BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)

}  // namespace base::android

#endif  // BASE_ANDROID_BACKGROUND_THREAD_POOL_FIELD_TRIAL_H_
