// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_LAUNCH_OPTIONS_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_LAUNCH_OPTIONS_H_

#include "base/command_line.h"
#include "base/files/file.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace borealis {

// Switch for adding an extra disk to be used with borealis.
extern const char kExtraDiskSwitch[];

class BorealisLaunchOptions {
 public:
  // Creates a per-profile instance of the launch options manager for Borealis.
  explicit BorealisLaunchOptions(Profile* profile);

  absl::optional<base::FilePath> GetExtraDisk();

 private:
  base::CommandLine GetLaunchOptions();

  Profile* const profile_;
  base::CommandLine launch_options_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_LAUNCH_OPTIONS_H_
