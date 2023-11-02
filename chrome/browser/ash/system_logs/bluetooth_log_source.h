// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_BLUETOOTH_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_BLUETOOTH_LOG_SOURCE_H_

#include "base/values.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

class BluetoothLogSource : public SystemLogsSource {
 public:
  BluetoothLogSource();
  BluetoothLogSource(const BluetoothLogSource&) = delete;
  BluetoothLogSource& operator=(const BluetoothLogSource&) = delete;
  ~BluetoothLogSource() override;

  // SystemLogsSource:
  void Fetch(SysLogsSourceCallback request) override;
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_BLUETOOTH_LOG_SOURCE_H_
