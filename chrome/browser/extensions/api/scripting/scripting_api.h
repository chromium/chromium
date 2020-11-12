// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SCRIPTING_SCRIPTING_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SCRIPTING_SCRIPTING_API_H_

#include <string>

#include "extensions/browser/extension_function.h"

class GURL;

namespace base {
class ListValue;
}

namespace extensions {

class ScriptingExecuteScriptFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("scripting.executeScript", SCRIPTING_EXECUTESCRIPT)

  ScriptingExecuteScriptFunction();
  ScriptingExecuteScriptFunction(const ScriptingExecuteScriptFunction&) =
      delete;
  ScriptingExecuteScriptFunction& operator=(
      const ScriptingExecuteScriptFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnScriptExecuted(const std::string& error,
                        const GURL& frame_url,
                        const base::ListValue& result);

  ~ScriptingExecuteScriptFunction() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SCRIPTING_SCRIPTING_API_H_
