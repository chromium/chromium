// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BLUETOOTH_DEBUG_LOGS_MANAGER_H_
#define CHROME_BROWSER_ASH_BLUETOOTH_DEBUG_LOGS_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals.mojom.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class PrefService;
class PrefRegistrySimple;

namespace ash {

namespace bluetooth {

// Manages the use of debug Bluetooth logs. Under normal usage, only warning and
// error logs are captured, but there are some situations in which it is
// advantageous to capture more verbose logs (e.g., in noisy environments or on
// a device with a particular Bluetooth chip). This class tracks the current
// state of debug logs and handles the user enabling/disabling them.
// This class also manages Bluetooth link quality debug logs and handles the
// enabling/disabling them.
// TODO(b/240788483) Fix the DebugLogsManager class name to include both the
// debug log and the link quality report.
class DebugLogsManager : public mojom::DebugLogsChangeHandler,
                         public device::BluetoothAdapter::Observer {
 public:
  DebugLogsManager(const std::string& primary_user_email,
                   PrefService* pref_service);

  DebugLogsManager(const DebugLogsManager&) = delete;
  DebugLogsManager& operator=(const DebugLogsManager&) = delete;

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

  // Generates an PendingRemote bound to this object.
  mojo::PendingRemote<mojom::DebugLogsChangeHandler> GenerateRemote();

  // Overrides for device::BluetoothAdapter::Observer:
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

 private:
  bool AreDebugLogsSupported() const;
  void SetVerboseLogsEnable(bool enable);
  void SendDBusVerboseLogsMessage(bool enable, int num_completed_attempts);
  void OnFlossSetDebugLogging(const bool enable,
                              const int num_completed_attempts,
                              floss::DBusResult<floss::Void> result);
  void OnVerboseLogsEnableSuccess(const bool enable);
  void OnVerboseLogsEnableError(const bool enable,
                                const int num_completed_attempts,
                                const std::string& error_name,
                                const std::string& error_message);
  void SetBluetoothQualityReport(bool enable, int num_completed_attempts);
  void OnSetBluetoothQualityReportSuccess(bool enable);
  void OnSetBluetoothQualityReportError(const bool enable,
                                        const int num_completed_attempts,
                                        const std::string& error_name,
                                        const std::string& error_message);

  // Called when an instance of |device::BluetoothAdapter| is ready. Does not
  // imply that an adapter is either present or powered.
  void OnBluetoothAdapterAvailable(
      scoped_refptr<device::BluetoothAdapter> adapter);

  const std::string primary_user_email_;
  raw_ptr<PrefService> pref_service_ = nullptr;
  mojo::ReceiverSet<mojom::DebugLogsChangeHandler> receivers_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  bool debug_logs_enabled_ = false;

  base::WeakPtrFactory<DebugLogsManager> weak_ptr_factory_{this};
};

}  // namespace bluetooth

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BLUETOOTH_DEBUG_LOGS_MANAGER_H_
