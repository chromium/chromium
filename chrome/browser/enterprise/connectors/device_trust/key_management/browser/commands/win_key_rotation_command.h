// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_WIN_KEY_ROTATION_COMMAND_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_WIN_KEY_ROTATION_COMMAND_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/win/windows_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace enterprise_connectors {

class WinKeyRotationCommand : public KeyRotationCommand {
 public:
  // Error returned by Omaha when concurrent elevated commands are run.
  // Making this public to be used in tests.
  static const HRESULT GOOPDATE_E_APP_USING_EXTERNAL_UPDATER = 0xA043081D;

  using RunGoogleUpdateElevatedCommandFn =
      base::RepeatingCallback<HRESULT(const wchar_t* command,
                                      const std::vector<std::string>& args,
                                      std::optional<DWORD>* return_code)>;

  // The second constructor is used in tests to override the behaviour of
  // Google Update.
  WinKeyRotationCommand();
  explicit WinKeyRotationCommand(
      RunGoogleUpdateElevatedCommandFn run_elevated_command);
  WinKeyRotationCommand(
      RunGoogleUpdateElevatedCommandFn run_elevated_command,
      scoped_refptr<base::SingleThreadTaskRunner> com_thread_runner);
  ~WinKeyRotationCommand() override;

  // KeyRotationCommand:
  void Trigger(const Params& params, Callback callback) override;

  // Enable or disable wait/sleep in tests to keep them from taking too long.
  void enable_waiting_for_testing(bool enabled) { waiting_enabled_ = enabled; }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> com_thread_runner_;
  bool waiting_enabled_ = true;
  RunGoogleUpdateElevatedCommandFn run_elevated_command_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_WIN_KEY_ROTATION_COMMAND_H_
