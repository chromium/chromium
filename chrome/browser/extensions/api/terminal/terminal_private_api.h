// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_PRIVATE_API_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/chromeos/crostini/crostini_simple_types.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/value_store/value_store.h"

class PrefChangeRegistrar;

namespace extensions {

class CrostiniStartupStatus;

class TerminalPrivateAPI : public BrowserContextKeyedAPI {
 public:
  explicit TerminalPrivateAPI(content::BrowserContext* context);
  ~TerminalPrivateAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<TerminalPrivateAPI>*
  GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<TerminalPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "TerminalPrivateAPI"; }

  content::BrowserContext* const context_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(TerminalPrivateAPI);
};

// Opens new terminal process. Returns the new terminal id.
class TerminalPrivateOpenTerminalProcessFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.openTerminalProcess",
                             TERMINALPRIVATE_OPENTERMINALPROCESS)

 protected:
  ~TerminalPrivateOpenTerminalProcessFunction() override;

  ExtensionFunction::ResponseAction Run() override;

  // Open the specified |process_name| with supplied |args|.
  ExtensionFunction::ResponseAction OpenProcess(
      const std::string& process_name,
      std::unique_ptr<std::vector<std::string>> args);

 private:
  void OnCrostiniRestarted(
      std::unique_ptr<CrostiniStartupStatus> startup_status,
      const std::string& user_id_hash,
      base::CommandLine cmdline,
      crostini::CrostiniResult result);

  void OpenVmshellProcess(const std::string& user_id_hash,
                          base::CommandLine cmdline);

  void OnGetVshSession(const std::string& user_id_hash,
                       base::CommandLine cmdline,
                       const std::string& terminal_id,
                       bool success,
                       const std::string& failure_reason,
                       int32_t container_shell_pid);

  void OpenProcess(const std::string& user_id_hash,
                   base::CommandLine cmdline);

  using ProcessOutputCallback =
      base::RepeatingCallback<void(const std::string& terminal_id,
                                   const std::string& output_type,
                                   const std::string& output)>;
  using OpenProcessCallback =
      base::OnceCallback<void(bool success, const std::string& terminal_id)>;
  void OpenOnRegistryTaskRunner(ProcessOutputCallback output_callback,
                                OpenProcessCallback callback,
                                base::CommandLine cmdline,
                                const std::string& user_id_hash);
  void RespondOnUIThread(bool success, const std::string& terminal_id);
};

// Opens new vmshell process. Returns the new terminal id.
class TerminalPrivateOpenVmshellProcessFunction
    : public TerminalPrivateOpenTerminalProcessFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.openVmshellProcess",
                             TERMINALPRIVATE_OPENVMSHELLPROCESS)

 protected:
  ~TerminalPrivateOpenVmshellProcessFunction() override;

  ExtensionFunction::ResponseAction Run() override;
};

// Send input to the terminal process specified by the terminal ID, which is set
// as an argument.
class TerminalPrivateSendInputFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.sendInput",
                             TERMINALPRIVATE_SENDINPUT)

 protected:
  ~TerminalPrivateSendInputFunction() override;

  ExtensionFunction::ResponseAction Run() override;

 private:
  void SendInputOnRegistryTaskRunner(const std::string& terminal_id,
                                     const std::string& input);
  void RespondOnUIThread(bool success);
};

// Closes terminal process.
class TerminalPrivateCloseTerminalProcessFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.closeTerminalProcess",
                             TERMINALPRIVATE_CLOSETERMINALPROCESS)

 protected:
  ~TerminalPrivateCloseTerminalProcessFunction() override;

  ExtensionFunction::ResponseAction Run() override;

 private:
  void CloseOnRegistryTaskRunner(const std::string& terminal_id);
  void RespondOnUIThread(bool success);
};

// Called by extension when terminal size changes.
class TerminalPrivateOnTerminalResizeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.onTerminalResize",
                             TERMINALPRIVATE_ONTERMINALRESIZE)

 protected:
  ~TerminalPrivateOnTerminalResizeFunction() override;

  ExtensionFunction::ResponseAction Run() override;

 private:
  void OnResizeOnRegistryTaskRunner(const std::string& terminal_id,
                                    int width,
                                    int height);
  void RespondOnUIThread(bool success);
};

class TerminalPrivateAckOutputFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.ackOutput",
                             TERMINALPRIVATE_ACKOUTPUT)

 protected:
  ~TerminalPrivateAckOutputFunction() override;

  ExtensionFunction::ResponseAction Run() override;

 private:
  void AckOutputOnRegistryTaskRunner(const std::string& terminal_id);
};

class TerminalPrivateOpenWindowFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.openWindow",
                             TERMINALPRIVATE_OPENWINDOW)

 protected:
  ~TerminalPrivateOpenWindowFunction() override;

  ExtensionFunction::ResponseAction Run() override;
};

class TerminalPrivateOpenOptionsPageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.openOptionsPage",
                             TERMINALPRIVATE_OPENOPTIONSPAGE)

 protected:
  ~TerminalPrivateOpenOptionsPageFunction() override;

  ExtensionFunction::ResponseAction Run() override;
};

class TerminalPrivateGetSettingsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.getSettings",
                             TERMINALPRIVATE_GETSETTINGS)

 protected:
  ~TerminalPrivateGetSettingsFunction() override;

  ExtensionFunction::ResponseAction Run() override;
};

class TerminalPrivateSetSettingsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.setSettings",
                             TERMINALPRIVATE_SETSETTINGS)

 protected:
  ~TerminalPrivateSetSettingsFunction() override;

  ExtensionFunction::ResponseAction Run() override;
};

class TerminalPrivateGetA11yStatusFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.getA11yStatus",
                             TERMINALPRIVATE_GETA11YSTATUS)

 protected:
  ~TerminalPrivateGetA11yStatusFunction() override;

  ExtensionFunction::ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_PRIVATE_API_H_
