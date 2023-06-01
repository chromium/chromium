// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_launch_options.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "third_party/re2/src/re2/re2.h"

namespace borealis {
namespace {

const char kExtraDiskSwitch[] = "extra-disk";
const char kAutoShutdownSwitch[] = "auto-shutdown";

bool IsDeveloperMode() {
  std::string output;
  if (!base::GetAppOutput({"/usr/bin/crossystem", "cros_debug"}, &output)) {
    return false;
  }
  return output == "1";
}

// Parses the borealis options string into a struct as per the comma-separated
// "key=value" list described in the borealis_launch_options.h file.
//
// For historic reasons: "--foo=bar" is treated as "foo=bar" and ";" (semicolon)
// is treated as an additional separator to ",".
BorealisLaunchOptions::Options ParseOptions(const std::string& options_string) {
  // Only devs/tests are allowed to modify these.
  if (!IsDeveloperMode()) {
    return {};
  }
  LOG(WARNING) << "Overriding borealis options with: " << options_string;
  BorealisLaunchOptions::Options opts;
  // We parse key=value pairs using regex:
  //  - "-*" consumes leading "-"s, for compatibility with the old format
  //  - "([^-][^=]*)" matches the key
  //  - "(.*)" matches the value
  RE2 pattern("-*([^-][^=]*)=(.*)");
  for (std::string option :
       base::SplitString(options_string, ";,", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    std::string key, val;
    if (!RE2::FullMatch(option, pattern, &key, &val))
      continue;
    if (key == kExtraDiskSwitch) {
      opts.extra_disk = base::FilePath(val);
    } else if (key == kAutoShutdownSwitch) {
      opts.auto_shutdown = base::ToLowerASCII(val[0]) == 't';
    }
  }
  return opts;
}

}  // namespace

BorealisLaunchOptions::Options::Options() = default;
BorealisLaunchOptions::Options::Options(const Options&) = default;
BorealisLaunchOptions::Options::~Options() = default;

BorealisLaunchOptions::BorealisLaunchOptions(Profile* profile)
    : options_(profile->GetPrefs()->GetString(prefs::kExtraLaunchOptions)) {}

void BorealisLaunchOptions::Build(
    base::OnceCallback<void(BorealisLaunchOptions::Options)> callback) const {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::MayBlock(), base::BindOnce(&ParseOptions, options_),
      std::move(callback));
}

void BorealisLaunchOptions::ForceOptions(std::string options) {
  options_ = options;
}

}  // namespace borealis
