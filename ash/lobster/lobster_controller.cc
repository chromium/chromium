// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_controller.h"

#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/hash/sha1.h"

namespace ash {
namespace {

constexpr std::string_view kLobsterKey(
    "\xB3\x3A\x4C\xFC\x84\xA0\x2B\xBE\xAC\x88\x48\x09\xCF\x5E\xD6\xD9\x28\xEC"
    "\x20\x2A",
    base::kSHA1Length);

}  // namespace

bool LobsterController::IsEnabled() {
  // Command line looks like:
  //  out/Default/chrome --user-data-dir=/tmp/tmp123
  //  --lobster-feature-key="INSERT KEY HERE" --enable-features=Lobster
  static const bool is_enabled =
      base::SHA1HashString(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kLobsterFeatureKey)) == kLobsterKey;
  return is_enabled;
}

}  // namespace ash
