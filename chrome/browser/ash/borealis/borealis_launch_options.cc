// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_launch_options.h"

#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

#include "base/logging.h"

namespace borealis {

const char kExtraDiskSwitch[] = "extra-disk";

BorealisLaunchOptions::BorealisLaunchOptions(Profile* profile)
    : profile_(profile), launch_options_(GetLaunchOptions()) {}

absl::optional<base::FilePath> BorealisLaunchOptions::GetExtraDisk() {
  if (launch_options_.HasSwitch(kExtraDiskSwitch)) {
    return base::FilePath(
        launch_options_.GetSwitchValueASCII(kExtraDiskSwitch));
  }
  return absl::nullopt;
}

base::CommandLine BorealisLaunchOptions::GetLaunchOptions() {
  // We prepend 'empty' aas base::CommandLine expects argv in the format of
  // { program, [(--|-|/)switch[=value]]*, [--], [argument]* }.
  std::string raw_string = base::StrCat(
      {"empty;", profile_->GetPrefs()->GetString(prefs::kExtraLaunchOptions)});
  std::vector<std::string> key_value_strings = base::SplitString(
      raw_string, ";", base::WhitespaceHandling::KEEP_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  return base::CommandLine(key_value_strings);
}

}  // namespace borealis
