// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLATFORM_EXPERIENCE_INSTALLER_INSTALLER_WIN_H_
#define CHROME_BROWSER_PLATFORM_EXPERIENCE_INSTALLER_INSTALLER_WIN_H_

#include <wrl/client.h>

#include "base/process/process.h"

struct IAppCommandWeb;

namespace base {
class CommandLine;
struct LaunchOptions;
}  // namespace base

namespace platform_experience {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PlatformExperienceHelperInstallState)
enum class PlatformExperienceHelperInstallState {
  kNotInstalled = 0,
  kHealthy = 1,
  kBinaryNoRegistration = 2,
  kVersionMismatch = 3,
  kRegisteredNoBinary = 4,
  kRegisteredZeroVersion = 5,
  kBinaryRegisteredZeroVersion = 6,
  kMaxValue = kBinaryRegisteredZeroVersion,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/windows/enums.xml:PlatformExperienceHelperInstallState)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PlatformExperienceHelperOtherScopeRegistration)
enum class PlatformExperienceHelperOtherScopeRegistration {
  kNotRegistered = 0,
  kRegisteredZeroVersion = 1,
  kRegisteredWithVersion = 2,
  kMaxValue = kRegisteredWithVersion,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/windows/enums.xml:PlatformExperienceHelperOtherScopeRegistration)

// Delegate for controlling how the Platform Experience Helper installer is
// launched.
class InstallerLauncherDelegate {
 public:
  virtual ~InstallerLauncherDelegate() = default;

  virtual Microsoft::WRL::ComPtr<IAppCommandWeb> GetUpdaterAppCommand(
      const std::wstring& command_name) = 0;

  virtual base::Process LaunchProcess(const base::CommandLine& cmd_line,
                                      const base::LaunchOptions& options) = 0;
};

void SetInstallerLauncherDelegateForTesting(
    InstallerLauncherDelegate* delegate);

// Starts the installation of the PEH, if it hasn't already been installed yet.
// This function might block.
void MaybeInstallPlatformExperienceHelper();

}  // namespace platform_experience

#endif  // CHROME_BROWSER_PLATFORM_EXPERIENCE_INSTALLER_INSTALLER_WIN_H_
