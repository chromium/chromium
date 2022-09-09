// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/bluetooth_log_source.h"

#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/floss/floss_features.h"

namespace system_logs {

namespace {

constexpr char kBluetoothFlossLogEntry[] = "CHROMEOS_BLUETOOTH_FLOSS";

}  // namespace

BluetoothLogSource::BluetoothLogSource() : SystemLogsSource("BluetoothLog") {}

BluetoothLogSource::~BluetoothLogSource() {}

void BluetoothLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  auto response = std::make_unique<SystemLogsResponse>();

  (*response)[kBluetoothFlossLogEntry] =
      floss::features::IsFlossEnabled() ? "enabled" : "disabled";
  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs
