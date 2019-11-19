// Copyright 2019 The Chromium Authors. All rights reserved.
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
  ~ChromeSerialDelegate() override;

  std::unique_ptr<content::SerialChooser> RunChooser(
      content::RenderFrameHost* frame,
      std::vector<blink::mojom::SerialPortFilterPtr> filters,
      content::SerialChooser::Callback callback) override;
  bool CanRequestPortPermission(content::RenderFrameHost* frame) override;
  bool HasPortPermission(content::RenderFrameHost* frame,
                         const device::mojom::SerialPortInfo& port) override;
  device::mojom::SerialPortManager* GetPortManager(
      content::RenderFrameHost* frame) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeSerialDelegate);
};

#endif  // CHROME_BROWSER_SERIAL_CHROME_SERIAL_DELEGATE_H_
