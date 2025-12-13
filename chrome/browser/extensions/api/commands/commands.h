// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_COMMANDS_COMMANDS_H_
#define CHROME_BROWSER_EXTENSIONS_API_COMMANDS_COMMANDS_H_

#include "extensions/browser/extension_function.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class GetAllCommandsFunction : public ExtensionFunction {
  ~GetAllCommandsFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("commands.getAll", COMMANDS_GETALL)
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_COMMANDS_COMMANDS_H_
