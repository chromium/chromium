// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_FILES_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_FILES_H_

#include "base/callback.h"
#include "base/files/file_path.h"

class Profile;

namespace plugin_vm {

// Ensure default shared dir <cryptohome>/MyFiles/PluginVm exists. Invokes
// |callback| with dir and true if dir is successfully created or already
// exists.
void EnsureDefaultSharedDirExists(
    Profile* profile,
    base::OnceCallback<void(const base::FilePath&, bool)> callback);

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_FILES_H_
