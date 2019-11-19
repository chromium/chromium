// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_CHROME_HID_DELEGATE_H_
#define CHROME_BROWSER_HID_CHROME_HID_DELEGATE_H_

#include <memory>
#include <vector>

#include "content/public/browser/hid_delegate.h"

class ChromeHidDelegate : public content::HidDelegate {
 public:
  ChromeHidDelegate();
  ~ChromeHidDelegate() override;

  std::unique_ptr<content::HidChooser> RunChooser(
      content::RenderFrameHost* frame,
      std::vector<blink::mojom::HidDeviceFilterPtr> filters,
      content::HidChooser::Callback callback) override;
  bool CanRequestDevicePermission(
      content::WebContents* web_contents,
      const url::Origin& requesting_origin) override;
  bool HasDevicePermission(content::WebContents* web_contents,
                           const url::Origin& requesting_origin,
                           const device::mojom::HidDeviceInfo& device) override;
  device::mojom::HidManager* GetHidManager(
      content::WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeHidDelegate);
};

#endif  // CHROME_BROWSER_HID_CHROME_HID_DELEGATE_H_
