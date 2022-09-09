// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_FILES_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_FILES_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class Profile;

namespace plugin_vm {

base::FilePath ChromeOSBaseDirectory();

// Ensure default shared dir <cryptohome>/MyFiles/PluginVm exists. Invokes
// |callback| with dir and true if dir is successfully created or already
// exists.
void EnsureDefaultSharedDirExists(
    Profile* profile,
    base::OnceCallback<void(const base::FilePath&, bool)> callback);

enum class LaunchPluginVmAppResult {
  SUCCESS,
  FAILED,
  FAILED_DIRECTORY_NOT_SHARED,
};

using LaunchArg = absl::variant<storage::FileSystemURL, std::string>;

using LaunchPluginVmAppCallback =
    base::OnceCallback<void(LaunchPluginVmAppResult result,
                            const std::string& failure_reason)>;

// Launch a Plugin VM App with a given set of files, given as cracked urls in
// the VM. Will start Plugin VM if it is not already running.
void LaunchPluginVmApp(Profile* profile,
                       std::string app_id,
                       const std::vector<LaunchArg>& files,
                       LaunchPluginVmAppCallback callback);

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_FILES_H_
