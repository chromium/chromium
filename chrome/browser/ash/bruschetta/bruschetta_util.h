// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_UTIL_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_UTIL_H_

#include "base/files/file_path.h"
#include "chrome/browser/ash/guest_os/guest_id.h"

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace bruschetta {

extern const char kBruschettaVmName[];
extern const char kBruschettaDisplayName[];

extern const char kBiosPath[];

enum class BruschettaResult {
  kUnknown,
  kSuccess,
  kDlcInstallError,
  kBiosNotAccessible,
  kStartVmFailed,
  kTimeout,
};

// Returns the string name of the BruschettaResult.
const char* BruschettaResultString(const BruschettaResult res);

guest_os::GuestId GetBruschettaAlphaId();

guest_os::GuestId MakeBruschettaId(std::string vm_name);

absl::optional<const base::Value::Dict*> GetRunnableConfig(
    const Profile* profile,
    const std::string& config_id);

base::FilePath BruschettaChromeOSBaseDirectory();

absl::optional<const base::Value::Dict*> GetInstallableConfig(
    const Profile* profile,
    const std::string& config_id);

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_UTIL_H_
