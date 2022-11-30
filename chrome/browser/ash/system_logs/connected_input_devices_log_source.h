// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_CONNECTED_INPUT_DEVICES_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_CONNECTED_INPUT_DEVICES_LOG_SOURCE_H_

#include <string>

#include "components/feedback/system_logs/system_logs_source.h"
#include "ui/events/devices/input_device.h"

namespace system_logs {

class ConnectedInputDevicesLogSource : public SystemLogsSource {
 public:
  ConnectedInputDevicesLogSource() : SystemLogsSource("Input") {}
  ConnectedInputDevicesLogSource(const ConnectedInputDevicesLogSource&) =
      delete;
  ConnectedInputDevicesLogSource& operator=(
      const ConnectedInputDevicesLogSource&) = delete;
  ~ConnectedInputDevicesLogSource() override = default;
  // Overridden from SystemLogsSource:
  void Fetch(SysLogsSourceCallback callback) override;

 private:
  void ProcessDeviceFillResponse(const ui::InputDevice dev,
                                 SystemLogsResponse* response,
                                 const std::string& vendor_str,
                                 const std::string& pid_str);
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_CONNECTED_INPUT_DEVICES_LOG_SOURCE_H_
