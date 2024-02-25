// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_PRIVATE_API_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

class PrefChangeRegistrar;

namespace guest_os {
struct GuestId;
}  // namespace guest_os

namespace extensions {

class StartupStatus;

class TerminalPrivateAPI : public BrowserContextKeyedAPI {
 public:
  explicit TerminalPrivateAPI(content::BrowserContext* context);

  TerminalPrivateAPI(const TerminalPrivateAPI&) = delete;
  TerminalPrivateAPI& operator=(const TerminalPrivateAPI&) = delete;

  ~TerminalPrivateAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<TerminalPrivateAPI>*
  GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<TerminalPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "TerminalPrivateAPI"; }

  const raw_ptr<content::BrowserContext> context_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

// Opens new terminal process. Returns the new terminal id.
class TerminalPrivateOpenTerminalProcessFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.openTerminalProcess",
                             TERMINALPRIVATE_OPENTERMINALPROCESS)
  TerminalPrivateOpenTerminalProcessFunction();

 protected:
  ~TerminalPrivateOpenTerminalProcessFunction() override;

  ExtensionFunction::ResponseAction Run() override;

  // Open the specified |process_name| with supplied |args|.
  ExtensionFunction::ResponseAction OpenProcess(
      const std::string& process_name,
      std::optional<std::vector<std::string>> args);

 private:
  void OnGuestRunning(const std::string& user_id_hash,
                      base::CommandLine cmdline,
                      bool success,
                      std::string failure_reason);

  void OpenVmshellProcess(const std::string& user_id_hash,
                          base::CommandLine cmdline);

  void OnGetVshSession(
      const std::string& user_id_hash,
      base::CommandLine cmdline,
      const std::string& terminal_id,
      std::optional<vm_tools::cicerone::GetVshSessionResponse>);

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
  std::unique_ptr<StartupStatus> startup_status_;
  std::unique_ptr<guest_os::GuestId> guest_id_;
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
  void OnSendInput(bool success);
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

class TerminalPrivateOpenSettingsSubpageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.openSettingsSubpage",
                             TERMINALPRIVATE_OPENSETTINGSSUBPAGE)

 protected:
  ~TerminalPrivateOpenSettingsSubpageFunction() override;

  ExtensionFunction::ResponseAction Run() override;
};

class TerminalPrivateGetOSInfoFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.getOSInfo",
                             TERMINALPRIVATE_GETOSINFO)

 protected:
  ~TerminalPrivateGetOSInfoFunction() override;

  ExtensionFunction::ResponseAction Run() override;
};

class TerminalPrivateGetPrefsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.getPrefs",
                             TERMINALPRIVATE_GETPREFS)

 protected:
  ~TerminalPrivateGetPrefsFunction() override;

  ExtensionFunction::ResponseAction Run() override;
};

class TerminalPrivateSetPrefsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.setPrefs",
                             TERMINALPRIVATE_SETPREFS)

 protected:
  ~TerminalPrivateSetPrefsFunction() override;

  ExtensionFunction::ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_PRIVATE_API_H_
