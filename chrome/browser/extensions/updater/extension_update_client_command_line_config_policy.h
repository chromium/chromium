// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATE_CLIENT_COMMAND_LINE_CONFIG_POLICY_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATE_CLIENT_COMMAND_LINE_CONFIG_POLICY_H_

#include "components/update_client/command_line_config_policy.h"

namespace base {
class CommandLine;
}

namespace extensions {

// This class implements the command line policy for the extension updater
// using update client.
class ExtensionUpdateClientCommandLineConfigPolicy final
    : public update_client::CommandLineConfigPolicy {
 public:
  explicit ExtensionUpdateClientCommandLineConfigPolicy(
      const base::CommandLine* cmdline);

  ExtensionUpdateClientCommandLineConfigPolicy(
      const ExtensionUpdateClientCommandLineConfigPolicy&) = delete;
  ExtensionUpdateClientCommandLineConfigPolicy& operator=(
      const ExtensionUpdateClientCommandLineConfigPolicy&) = delete;

  // update_client::CommandLineConfigPolicy overrides.
  bool TestRequest() const override;

 private:
  bool test_request_ = false;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATE_CLIENT_COMMAND_LINE_CONFIG_POLICY_H_
