// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/command_line_private/command_line_private_api.h"

#include <string>

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/common/extensions/api/command_line_private.h"

namespace {
// This should be consistent with
// chrome/test/data/extensions/api_test/command_line/basics/test.js.
const char kEmptySwitchName[] = "Switch name is empty.";
}  // namespace

namespace extensions {

namespace command_line_private = api::command_line_private;

ExtensionFunction::ResponseAction CommandLinePrivateHasSwitchFunction::Run() {
  std::optional<command_line_private::HasSwitch::Params> params =
      command_line_private::HasSwitch::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->name.empty()) {
    return RespondNow(Error(kEmptySwitchName));
  }

  return RespondNow(
      ArgumentList(command_line_private::HasSwitch::Results::Create(
          base::CommandLine::ForCurrentProcess()->HasSwitch(params->name))));
}

}  // namespace extensions
