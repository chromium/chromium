// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Debugger API functions for attaching debugger
// to the page.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_DEBUGGER_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_DEBUGGER_API_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/timer/timer.h"
#include "chrome/common/extensions/api/debugger.h"
#include "content/public/browser/devtools_agent_host.h"
#include "extensions/browser/extension_function.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using extensions::api::debugger::Debuggee;
using extensions::api::debugger::DebuggerSession;

struct MessageSubstitution;

// Base debugger function.

namespace extensions {
class ExtensionDevToolsClientHost;

// Android uses messages instead of desktop infobars, so the desktop infobar
// controller is skipped on Android.
#if !BUILDFLAG(IS_ANDROID)
class ExtensionDevToolsInfoBarController {
 public:
  static ExtensionDevToolsInfoBarController* GetInstance();

  // Returns the message substitution containing the active extension's name for
  // the Extension DevTools infobar message template.
  static std::vector<MessageSubstitution> GetMessageSubstitutions();

  // Called when the user clicks Cancel or dismisses the infobar. Detaches all
  // active client hosts to terminate active debugging sessions.
  static void OnInfoBarAction();

  void OnClientHostAttached(ExtensionDevToolsClientHost* host,
                            const std::string& extension_name);
  void OnClientHostDetached(ExtensionDevToolsClientHost* host);

 private:
  friend class base::NoDestructor<ExtensionDevToolsInfoBarController>;
  ExtensionDevToolsInfoBarController();
  ~ExtensionDevToolsInfoBarController();

  void OnInfoBarActionInternal();

  std::set<raw_ptr<ExtensionDevToolsClientHost, SetExperimental>> active_hosts_;
  std::u16string last_extension_name_;
  base::OneShotTimer autoclose_timer_;
};
#endif  // !BUILDFLAG(IS_ANDROID)

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
