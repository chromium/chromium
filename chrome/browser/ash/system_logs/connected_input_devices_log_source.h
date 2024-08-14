// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_CONNECTED_INPUT_DEVICES_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_CONNECTED_INPUT_DEVICES_LOG_SOURCE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/events/devices/input_device.h"

namespace system_logs {

class ConnectedInputDevicesLogSource : public SystemLogsSource {
 public:
  ConnectedInputDevicesLogSource();
  ConnectedInputDevicesLogSource(const ConnectedInputDevicesLogSource&) =
      delete;
  ConnectedInputDevicesLogSource& operator=(
      const ConnectedInputDevicesLogSource&) = delete;
  ~ConnectedInputDevicesLogSource() override;
  // Overridden from SystemLogsSource:
  void Fetch(SysLogsSourceCallback callback) override;

 private:
  void ProcessDeviceFillResponse(const ui::InputDevice dev,
                                 SystemLogsResponse* response,
                                 const std::string& vendor_str,
                                 const std::string& pid_str);

  void OnTelemetryInfoProbeResponse(
      base::OnceCallback<void(const std::string&, const std::string&)> callback,
      ash::cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  void OnDisconnect();

  ash::cros_healthd::mojom::CrosHealthdProbeService* GetCrosHealthdService();

  mojo::Remote<ash::cros_healthd::mojom::CrosHealthdProbeService>
      probe_service_;

  base::WeakPtrFactory<ConnectedInputDevicesLogSource> weak_ptr_factory_{this};
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_CONNECTED_INPUT_DEVICES_LOG_SOURCE_H_
