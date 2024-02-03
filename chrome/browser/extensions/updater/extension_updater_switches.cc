// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_updater_switches.h"

#include "base/command_line.h"
#include "chrome/common/channel_info.h"

namespace extensions {

const char kSwitchTestRequestParam[] = "extension-updater-test-request";

const char kSwitchExtensionForceChannel[] = "extension-force-channel";

std::string GetChannelForExtensionUpdates() {
  std::string forced_channel =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          extensions::kSwitchExtensionForceChannel);
  if (!forced_channel.empty()) {
    return forced_channel;
  }

  return chrome::GetChannelName(chrome::WithExtendedStable(true));
}

}  // namespace extensions
