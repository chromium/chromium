// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BLUETOOTH_DEBUG_LOGS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_BLUETOOTH_DEBUG_LOGS_MANAGER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

class PrefService;
class PrefRegistrySimple;

namespace chromeos {

namespace bluetooth {

// Manages the use of debug Bluetooth logs. Under normal usage, only warning and
// error logs are captured, but there are some situations in which it is
// advantageous to capture more verbose logs (e.g., in noisy environments or on
// a device with a particular Bluetooth chip). This class tracks the current
// state of debug logs and handles the user enabling/disabling them.
class DebugLogsManager : public mojom::DebugLogsChangeHandler {
 public:
  DebugLogsManager(const std::string& primary_user_email,
                   PrefService* pref_service);
  ~DebugLogsManager() override;

  // State for capturing debug Bluetooth logs; logs are only captured when
  // supported and enabled. Debug logs are supported when the associated flag is
  // enabled and if an eligible user is signed in. Debug logs are enabled only
  // via interaction with the DebugLogsChangeHandler Mojo interface.
  enum class DebugLogsState {
    kNotSupported,
    kSupportedButDisabled,
    kSupportedAndEnabled
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  DebugLogsState GetDebugLogsState() const;

  // mojom::DebugLogsManager:
  void ChangeDebugLogsState(bool should_debug_logs_be_enabled) override;

  // Generates an InterfacePtr bound to this object.
  mojom::DebugLogsChangeHandlerPtr GenerateInterfacePtr();

 private:
  bool AreDebugLogsSupported() const;

  const std::string primary_user_email_;
  PrefService* pref_service_ = nullptr;
  mojo::BindingSet<mojom::DebugLogsChangeHandler> bindings_;

  DISALLOW_COPY_AND_ASSIGN(DebugLogsManager);
};

}  // namespace bluetooth

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BLUETOOTH_DEBUG_LOGS_MANAGER_H_
