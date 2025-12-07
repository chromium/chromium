// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/updater/extension_update_client_command_line_config_policy.h"
#include "chrome/browser/extensions/updater/extension_updater_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

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
