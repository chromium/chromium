// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSVM_PROCESS_LIST_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSVM_PROCESS_LIST_H_

#include <set>
#include <unordered_map>

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/system/procfs_util.h"

namespace crostini {

using PidStatMap = std::unordered_map<pid_t, chromeos::system::SingleProcStat>;

// Returns a map from crosvm PIDs to their stat map.
// |slash_proc| is "/proc" for production and is only changed for tests.
PidStatMap GetCrosvmPidStatMap(
    const base::FilePath& slash_proc = base::FilePath("/proc"));

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSVM_PROCESS_LIST_H_
