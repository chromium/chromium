// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/delegated_tasks/peh_launcher.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/win/trust_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/install_static/install_util.h"

namespace platform_experience {

namespace {

constexpr wchar_t kPlatformExperienceHelperDir[] = L"PlatformExperienceHelper";
constexpr wchar_t kPlatformExperienceHelperExe[] =
    L"platform_experience_helper.exe";

}  // namespace

PehLauncher::PehLauncher() = default;

PehLauncher::~PehLauncher() = default;

base::FilePath PehLauncher::GetBinaryPath() {
  base::FilePath base_dir;
  base::PathService::Get(install_static::IsSystemInstall()
                             ? static_cast<int>(base::DIR_EXE)
                             : static_cast<int>(chrome::DIR_USER_DATA),
                         &base_dir);

  base::FilePath path = base_dir.Append(kPlatformExperienceHelperDir)
                            .Append(kPlatformExperienceHelperExe);
  if (!base::PathExists(path)) {
    return base::FilePath();
  }
  return path;
}

bool PehLauncher::IsBinaryVerified(const base::FilePath& binary_path) {
  return base::win::IsBinaryTrusted(binary_path);
}

base::Process PehLauncher::LaunchProcess(const base::CommandLine& cmd_line,
                                         const base::LaunchOptions& options) {
  return base::LaunchProcess(cmd_line, options);
}

}  // namespace platform_experience
