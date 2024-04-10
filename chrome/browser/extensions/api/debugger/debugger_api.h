// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Debugger API functions for attaching debugger
// to the page.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_DEBUGGER_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_DEBUGGER_API_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/common/extensions/api/debugger.h"
#include "content/public/browser/devtools_agent_host.h"
#include "extensions/browser/extension_function.h"

using extensions::api::debugger::Debuggee;
using extensions::api::debugger::DebuggerSession;

// Base debugger function.

namespace extensions {
class ExtensionDevToolsClientHost;

class DebuggerFunction : public ExtensionFunction {
 protected:
  DebuggerFunction();
  ~DebuggerFunction() override;

  std::string FormatErrorMessage(const std::string& format);

  bool InitAgentHost(std::string* error);
  bool InitClientHost(std::string* error);
  ExtensionDevToolsClientHost* FindClientHost();

  Debuggee debuggee_;
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  raw_ptr<ExtensionDevToolsClientHost, DanglingUntriaged> client_host_;
};

// Implements the debugger.attach() extension function.
class DebuggerAttachFunction : public DebuggerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("debugger.attach", DEBUGGER_ATTACH)

  DebuggerAttachFunction();

 protected:
  ~DebuggerAttachFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

// Implements the debugger.detach() extension function.
class DebuggerDetachFunction : public DebuggerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("debugger.detach", DEBUGGER_DETACH)

  DebuggerDetachFunction();

 protected:
  ~DebuggerDetachFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

// Implements the debugger.sendCommand() extension function.
class DebuggerSendCommandFunction : public DebuggerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("debugger.sendCommand", DEBUGGER_SENDCOMMAND)

  DebuggerSendCommandFunction();
  void SendResponseBody(base::Value result);
  void SendDetachedError();

 protected:
  ~DebuggerSendCommandFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DebuggerSession debugger_session_;
};

// Implements the debugger.getTargets() extension function.
class DebuggerGetTargetsFunction : public DebuggerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("debugger.getTargets", DEBUGGER_GETTARGETS)

  DebuggerGetTargetsFunction();

 protected:
  ~DebuggerGetTargetsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_DEBUGGER_API_H_
