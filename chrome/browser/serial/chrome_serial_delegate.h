// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERIAL_CHROME_SERIAL_DELEGATE_H_
#define CHROME_BROWSER_SERIAL_CHROME_SERIAL_DELEGATE_H_

#include <memory>
#include <vector>

#include "content/public/browser/serial_delegate.h"

class ChromeSerialDelegate : public content::SerialDelegate {
 public:
  ChromeSerialDelegate();

  ChromeSerialDelegate(const ChromeSerialDelegate&) = delete;
  ChromeSerialDelegate& operator=(const ChromeSerialDelegate&) = delete;

  ~ChromeSerialDelegate() override;

  std::unique_ptr<content::SerialChooser> RunChooser(
      content::RenderFrameHost* frame,
      std::vector<blink::mojom::SerialPortFilterPtr> filters,
      std::vector<device::BluetoothUUID> allowed_bluetooth_service_class_ids,
      content::SerialChooser::Callback callback) override;
  bool CanRequestPortPermission(content::RenderFrameHost* frame) override;
  bool HasPortPermission(content::RenderFrameHost* frame,
                         const device::mojom::SerialPortInfo& port) override;
  void RevokePortPermissionWebInitiated(
      content::RenderFrameHost* frame,
      const base::UnguessableToken& token) override;
  const device::mojom::SerialPortInfo* GetPortInfo(
      content::RenderFrameHost* frame,
      const base::UnguessableToken& token) override;
  device::mojom::SerialPortManager* GetPortManager(
      content::RenderFrameHost* frame) override;
  void AddObserver(content::RenderFrameHost* frame,
                   Observer* observer) override;
  void RemoveObserver(content::RenderFrameHost* frame,
                      Observer* observer) override;
};

#endif  // CHROME_BROWSER_SERIAL_CHROME_SERIAL_DELEGATE_H_
