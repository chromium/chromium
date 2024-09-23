// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BLUETOOTH_CHROME_BLUETOOTH_DELEGATE_H_
#define CHROME_BROWSER_BLUETOOTH_CHROME_BLUETOOTH_DELEGATE_H_

#include <memory>

#include "components/permissions/bluetooth_delegate_impl.h"

namespace content {
class RenderFrameHost;
}  // namespace content

class ChromeBluetoothDelegate : public permissions::BluetoothDelegateImpl {
 public:
  explicit ChromeBluetoothDelegate(std::unique_ptr<Client> client);
  bool MayUseBluetooth(content::RenderFrameHost* rfh) override;
};

#endif  // CHROME_BROWSER_BLUETOOTH_CHROME_BLUETOOTH_DELEGATE_H_
