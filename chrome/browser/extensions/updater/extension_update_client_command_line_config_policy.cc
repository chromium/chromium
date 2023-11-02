// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_update_client_command_line_config_policy.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/updater/extension_updater_switches.h"
#include "url/gurl.h"

namespace extensions {

ExtensionUpdateClientCommandLineConfigPolicy::
    ExtensionUpdateClientCommandLineConfigPolicy(
        const base::CommandLine* cmdline) {
  DCHECK(cmdline);
  test_request_ = cmdline->HasSwitch(kSwitchTestRequestParam);
}

bool ExtensionUpdateClientCommandLineConfigPolicy::TestRequest() const {
  return test_request_;
}

}  // namespace extensions
