// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/background_thread_pool_field_trial.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/base_switches.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/features.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock_impl.h"

namespace base {

namespace features {
BASE_FEATURE(kBackgroundThreadPoolFieldTrial, FEATURE_DISABLED_BY_DEFAULT);

// |kBackgroundThreadPoolFieldTrialConfig| is queried only by the Java layer
// using CachedFlags, so mark we mark it as unused to make the C++ compiler
// happy.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
BASE_FEATURE_PARAM(int,
                   kBackgroundThreadPoolFieldTrialConfig,
                   &kBackgroundThreadPoolFieldTrial,
                   "config",
                   0);
#pragma clang diagnostic pop
}  // namespace features

namespace android {

// The global cached configuration of the trial.
std::optional<BackgroundThreadPoolFieldTrial::Configuration>
    BackgroundThreadPoolFieldTrial::s_configuration_;

void BackgroundThreadPoolFieldTrial::Initialize() {
  s_configuration_ = ReadConfigurationFromCommandLine();
}

BackgroundThreadPoolFieldTrial::Configuration
BackgroundThreadPoolFieldTrial::ReadConfigurationFromCommandLine() {
  DCHECK(base::CommandLine::InitializedForCurrentProcess());

  std::string value_str = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kBackgroundThreadPoolFieldTrial);
  if (value_str.empty()) {
    return Configuration::kDisabled;
  }

  int value = -1;
  if (!base::StringToInt(value_str, &value) ||
      value < static_cast<int>(Configuration::kDisabled) ||
      value > static_cast<int>(Configuration::kConfigurationMax)) {
    return Configuration::kDisabled;
  }

  return Configuration(value);
}

ALWAYS_INLINE BackgroundThreadPoolFieldTrial::Configuration
BackgroundThreadPoolFieldTrial::GetConfiguration() {
  // For the few lock instances where the configuration is read very early, we
  // return |kDisabled|.
  return s_configuration_.has_value() ? Configuration(*s_configuration_)
                                      : Configuration::kDisabled;
}

#if BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)
BackgroundThreadPoolFieldTrial::TrialInfo
BackgroundThreadPoolFieldTrial::GetPISupportedTrialInfo() {
  constexpr std::string_view kVersionSuffix = "_20250917";
  std::string_view group_name;

  switch (GetConfiguration()) {
    case Configuration::kPISupportedTrialControl:
      group_name = "Control";
      break;
    case Configuration::kPISupportedTrialEnabledPILocksOnly:
      group_name = "EnabledPILocksOnly";
      break;
    case Configuration::kPISupportedTrialEnabledBGThreadPoolOnly:
      group_name = "EnabledBGThreadPoolOnly";
      break;
    case Configuration::kPISupportedTrialEnabledBoth:
      group_name = "EnabledBoth";
      break;
    default:
      LOG(FATAL) << "configuration value "
                 << static_cast<int>(GetConfiguration())
                 << " should not have called" << __PRETTY_FUNCTION__;
      __builtin_unreachable();
  }

  return TrialInfo("AndroidBackgroundThreadPoolPISupportedSynthetic",
                   base::StrCat({group_name, kVersionSuffix}));
}
#endif  // BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)

BackgroundThreadPoolFieldTrial::TrialInfo
BackgroundThreadPoolFieldTrial::GetGeneralTrialInfo() {
  constexpr std::string_view kVersionSuffix = "_20250505";
  std::string_view group_name;

  switch (GetConfiguration()) {
    case Configuration::kGeneralTrialControl:
      group_name = "Control";
      break;
    case Configuration::kGeneralTrialEnabledBGThreadPool:
      group_name = "Enabled";
      break;
    default:
      LOG(FATAL) << "configuration value "
                 << static_cast<int>(GetConfiguration())
                 << " should not have called" << __PRETTY_FUNCTION__;
      __builtin_unreachable();
  }

  return TrialInfo("AndroidBackgroundThreadPoolGeneralSynthetic",
                   base::StrCat({group_name, kVersionSuffix}));
}

bool BackgroundThreadPoolFieldTrial::ShouldUsePriorityInheritanceLocks() {
  switch (GetConfiguration()) {
#if BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)
    case Configuration::kPISupportedTrialEnabledPILocksOnly:
    case Configuration::kPISupportedTrialEnabledBoth:
      return base::KernelSupportsPriorityInheritanceFutex();
#endif  // BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)
    default:
      return false;
  }
}

bool BackgroundThreadPoolFieldTrial::ShouldUseBackgroundThreadPool() {
  switch (GetConfiguration()) {
#if BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)
    case Configuration::kPISupportedTrialEnabledBGThreadPoolOnly:
    case Configuration::kPISupportedTrialEnabledBoth:
      return base::KernelSupportsPriorityInheritanceFutex();
#endif  // BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)
    case Configuration::kGeneralTrialEnabledBGThreadPool:
      return true;
    default:
      return false;
  }
}

std::optional<BackgroundThreadPoolFieldTrial::TrialInfo>
BackgroundThreadPoolFieldTrial::GetTrialInfo() {
  switch (GetConfiguration()) {
#if BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)
    case Configuration::kPISupportedTrialControl:
    case Configuration::kPISupportedTrialEnabledPILocksOnly:
    case Configuration::kPISupportedTrialEnabledBGThreadPoolOnly:
    case Configuration::kPISupportedTrialEnabledBoth:
      return base::KernelSupportsPriorityInheritanceFutex()
                 ? std::make_optional(GetPISupportedTrialInfo())
                 : std::nullopt;
#endif  // BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)
    case Configuration::kGeneralTrialControl:
    case Configuration::kGeneralTrialEnabledBGThreadPool:
      return GetGeneralTrialInfo();
    default:
      return std::nullopt;
  }
}

#if BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)
ScopedUsePriorityInheritanceLocksForTesting::
    ScopedUsePriorityInheritanceLocksForTesting()
    : reset_config_value_(&BackgroundThreadPoolFieldTrial::s_configuration_,
                          BackgroundThreadPoolFieldTrial::Configuration::
                              kPISupportedTrialEnabledPILocksOnly) {}

ScopedUsePriorityInheritanceLocksForTesting::
    ~ScopedUsePriorityInheritanceLocksForTesting() = default;
#endif  // BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)
}  // namespace android

}  // namespace base
