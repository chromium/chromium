// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/terminal/terminal_private_api.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/terminal/terminal_extension_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/terminal_private.h"
#include "chromeos/process_proxy/process_proxy_registry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_browser_client.h"

namespace terminal_private = extensions::api::terminal_private;
namespace OnTerminalResize =
    extensions::api::terminal_private::OnTerminalResize;
namespace OpenTerminalProcess =
    extensions::api::terminal_private::OpenTerminalProcess;
namespace CloseTerminalProcess =
    extensions::api::terminal_private::CloseTerminalProcess;
namespace SendInput = extensions::api::terminal_private::SendInput;
namespace AckOutput = extensions::api::terminal_private::AckOutput;

namespace {

const char kCroshName[] = "crosh";
const char kCroshCommand[] = "/usr/bin/crosh";
// We make stubbed crosh just echo back input.
const char kStubbedCroshCommand[] = "cat";

const char kVmShellName[] = "vmshell";
const char kVmShellCommand[] = "/usr/bin/vsh";

std::string GetCroshPath() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kCroshCommand))
    return command_line->GetSwitchValueASCII(switches::kCroshCommand);

  if (base::SysInfo::IsRunningOnChromeOS())
    return std::string(kCroshCommand);

  return std::string(kStubbedCroshCommand);
}

// Get the program to run based on the openTerminalProcess JS request.
std::string GetProcessCommandForName(const std::string& name) {
  if (name == kCroshName)
    return GetCroshPath();
  else if (name == kVmShellName)
    return kVmShellCommand;
  else
    return std::string();
}

// Whether the program accepts arbitrary command line arguments.
bool CommandSupportsArguments(const std::string& name) {
  if (name == kVmShellName)
    return true;

  return false;
}

void NotifyProcessOutput(content::BrowserContext* browser_context,
                         const std::string& extension_id,
                         int tab_id,
                         int terminal_id,
                         const std::string& output_type,
                         const std::string& output) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&NotifyProcessOutput, browser_context, extension_id,
                       tab_id, terminal_id, output_type, output));
    return;
  }

  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->AppendInteger(tab_id);
  args->AppendInteger(terminal_id);
  args->AppendString(output_type);
  args->AppendString(output);

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(browser_context);
  if (event_router) {
    std::unique_ptr<extensions::Event> event(new extensions::Event(
        extensions::events::TERMINAL_PRIVATE_ON_PROCESS_OUTPUT,
        terminal_private::OnProcessOutput::kEventName, std::move(args)));
    event_router->DispatchEventToExtension(extension_id, std::move(event));
  }
}

// Returns tab ID, or window session ID (for platform apps) for |web_contents|.
int GetTabOrWindowSessionId(content::BrowserContext* browser_context,
                            content::WebContents* web_contents) {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  if (tab_id >= 0)
    return tab_id;
  extensions::AppWindow* window =
      extensions::AppWindowRegistry::Get(browser_context)
          ->GetAppWindowForWebContents(web_contents);
  return window ? window->session_id().id() : -1;
}

}  // namespace

namespace extensions {

TerminalPrivateOpenTerminalProcessFunction::
    TerminalPrivateOpenTerminalProcessFunction() {}

TerminalPrivateOpenTerminalProcessFunction::
    ~TerminalPrivateOpenTerminalProcessFunction() {}

ExtensionFunction::ResponseAction
TerminalPrivateOpenTerminalProcessFunction::Run() {
  std::unique_ptr<OpenTerminalProcess::Params> params(
      OpenTerminalProcess::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const std::string command = GetProcessCommandForName(params->process_name);
  if (command.empty())
    return RespondNow(Error("Invalid process name."));

  const std::string user_id_hash =
      ExtensionsBrowserClient::Get()->GetUserIdHashFromContext(
          browser_context());

  std::vector<std::string> arguments;
  arguments.push_back(command);
  if (params->args) {
    for (const std::string& arg : *params->args)
      arguments.push_back(arg);
  }

  if (arguments.size() > 1 && !CommandSupportsArguments(params->process_name))
    return RespondNow(Error("Specified command does not support arguments."));

  content::WebContents* caller_contents = GetSenderWebContents();
  if (!caller_contents)
    return RespondNow(Error("No web contents."));

  // Passed to terminalPrivate.ackOutput, which is called from the API's custom
  // bindings after terminalPrivate.onProcessOutput is dispatched. It is used to
  // determine whether ackOutput call should be handled or not. ackOutput will
  // be called from every web contents in which a onProcessOutput listener
  // exists (because the API custom bindings hooks are run in every web contents
  // with a listener). Only ackOutput called from the web contents that has the
  // target terminal instance should be handled.
  // TODO(tbarzic): Instead of passing tab/app window session id around, keep
  //     mapping from web_contents to terminal ID running in it. This will be
  //     needed to fix crbug.com/210295.
  int tab_id = GetTabOrWindowSessionId(browser_context(), caller_contents);
  if (tab_id < 0)
    return RespondNow(Error("Not called from a tab or app window"));

  // Registry lives on its own task runner.
  chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TerminalPrivateOpenTerminalProcessFunction::OpenOnRegistryTaskRunner,
          this,
          base::Bind(&NotifyProcessOutput, browser_context(), extension_id(),
                     tab_id),
          base::Bind(
              &TerminalPrivateOpenTerminalProcessFunction::RespondOnUIThread,
              this),
          arguments,
          user_id_hash));
  return RespondLater();
}

void TerminalPrivateOpenTerminalProcessFunction::OpenOnRegistryTaskRunner(
    const ProcessOutputCallback& output_callback,
    const OpenProcessCallback& callback,
    const std::vector<std::string>& arguments,
    const std::string& user_id_hash) {
  chromeos::ProcessProxyRegistry* registry =
      chromeos::ProcessProxyRegistry::Get();
  const base::CommandLine cmdline{arguments};

  int terminal_id =
      registry->OpenProcess(cmdline, user_id_hash, output_callback);

  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindOnce(callback, terminal_id));
}

TerminalPrivateSendInputFunction::~TerminalPrivateSendInputFunction() {}

void TerminalPrivateOpenTerminalProcessFunction::RespondOnUIThread(
    int terminal_id) {
  if (terminal_id < 0) {
    Respond(Error("Failed to open process."));
    return;
  }
  Respond(OneArgument(std::make_unique<base::Value>(terminal_id)));
}

ExtensionFunction::ResponseAction TerminalPrivateSendInputFunction::Run() {
  std::unique_ptr<SendInput::Params> params(SendInput::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Registry lives on its own task runner.
  chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TerminalPrivateSendInputFunction::SendInputOnRegistryTaskRunner,
          this, params->pid, params->input));
  return RespondLater();
}

void TerminalPrivateSendInputFunction::SendInputOnRegistryTaskRunner(
    int terminal_id,
    const std::string& text) {
  bool success =
      chromeos::ProcessProxyRegistry::Get()->SendInput(terminal_id, text);

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&TerminalPrivateSendInputFunction::RespondOnUIThread, this,
                     success));
}

void TerminalPrivateSendInputFunction::RespondOnUIThread(bool success) {
  Respond(OneArgument(std::make_unique<base::Value>(success)));
}

TerminalPrivateCloseTerminalProcessFunction::
    ~TerminalPrivateCloseTerminalProcessFunction() {}

ExtensionFunction::ResponseAction
TerminalPrivateCloseTerminalProcessFunction::Run() {
  std::unique_ptr<CloseTerminalProcess::Params> params(
      CloseTerminalProcess::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Registry lives on its own task runner.
  chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&TerminalPrivateCloseTerminalProcessFunction::
                                    CloseOnRegistryTaskRunner,
                                this, params->pid));

  return RespondLater();
}

void TerminalPrivateCloseTerminalProcessFunction::CloseOnRegistryTaskRunner(
    int terminal_id) {
  bool success =
      chromeos::ProcessProxyRegistry::Get()->CloseProcess(terminal_id);

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &TerminalPrivateCloseTerminalProcessFunction::RespondOnUIThread, this,
          success));
}

void TerminalPrivateCloseTerminalProcessFunction::RespondOnUIThread(
    bool success) {
  Respond(OneArgument(std::make_unique<base::Value>(success)));
}

TerminalPrivateOnTerminalResizeFunction::
    ~TerminalPrivateOnTerminalResizeFunction() {}

ExtensionFunction::ResponseAction
TerminalPrivateOnTerminalResizeFunction::Run() {
  std::unique_ptr<OnTerminalResize::Params> params(
      OnTerminalResize::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Registry lives on its own task runner.
  chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TerminalPrivateOnTerminalResizeFunction::
                         OnResizeOnRegistryTaskRunner,
                     this, params->pid, params->width, params->height));

  return RespondLater();
}

void TerminalPrivateOnTerminalResizeFunction::OnResizeOnRegistryTaskRunner(
    int terminal_id,
    int width,
    int height) {
  bool success = chromeos::ProcessProxyRegistry::Get()->OnTerminalResize(
      terminal_id, width, height);

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &TerminalPrivateOnTerminalResizeFunction::RespondOnUIThread, this,
          success));
}

void TerminalPrivateOnTerminalResizeFunction::RespondOnUIThread(bool success) {
  Respond(OneArgument(std::make_unique<base::Value>(success)));
}

TerminalPrivateAckOutputFunction::~TerminalPrivateAckOutputFunction() {}

ExtensionFunction::ResponseAction TerminalPrivateAckOutputFunction::Run() {
  std::unique_ptr<AckOutput::Params> params(AckOutput::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::WebContents* caller_contents = GetSenderWebContents();
  if (!caller_contents)
    return RespondNow(Error("No web contents."));

  int tab_id = GetTabOrWindowSessionId(browser_context(), caller_contents);
  if (tab_id < 0)
    return RespondNow(Error("Not called from a tab or app window"));

  if (tab_id != params->tab_id)
    return RespondNow(NoArguments());

  // Registry lives on its own task runner.
  chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TerminalPrivateAckOutputFunction::AckOutputOnRegistryTaskRunner,
          this, params->pid));

  return RespondNow(NoArguments());
}

void TerminalPrivateAckOutputFunction::AckOutputOnRegistryTaskRunner(
    int terminal_id) {
  chromeos::ProcessProxyRegistry::Get()->AckOutput(terminal_id);
}

}  // namespace extensions
