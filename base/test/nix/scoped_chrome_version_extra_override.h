// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_NIX_SCOPED_CHROME_VERSION_EXTRA_OVERRIDE_H_
#define BASE_TEST_NIX_SCOPED_CHROME_VERSION_EXTRA_OVERRIDE_H_

#include <optional>
#include <string>

#include "base/version_info/channel.h"
#include "build/build_config.h"

static_assert(BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC));

namespace base::test {

// Allows a test to override the browser's channel for a limited time (e.g., for
// the duration of a test). This minimally impacts the behavior of the functions
// in base/version_info/nix/version_extra_utils.h.
class ScopedChromeVersionExtraOverride {
 public:
  explicit ScopedChromeVersionExtraOverride(version_info::Channel channel,
                                            bool is_extended_stable = false);
  ~ScopedChromeVersionExtraOverride();

 private:
  // The original value of the CHROME_VERSION_EXTRA environment variable.
  std::optional<std::string> old_env_var_;
};

}  // namespace base::test

#endif  // BASE_TEST_NIX_SCOPED_CHROME_VERSION_EXTRA_OVERRIDE_H_
