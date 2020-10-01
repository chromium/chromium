// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_CHROME_HID_DELEGATE_H_
#define CHROME_BROWSER_HID_CHROME_HID_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "components/permissions/chooser_context_base.h"
#include "content/public/browser/hid_delegate.h"

class ChromeHidDelegate
    : public content::HidDelegate,
      public permissions::ChooserContextBase::PermissionObserver,
      public HidChooserContext::DeviceObserver {
 public:
  ChromeHidDelegate();
  ChromeHidDelegate(ChromeHidDelegate&) = delete;
  ChromeHidDelegate& operator=(ChromeHidDelegate&) = delete;
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
  void AddObserver(content::RenderFrameHost* frame,
                   content::HidDelegate::Observer* observer) override;
  void RemoveObserver(content::RenderFrameHost* frame,
                      content::HidDelegate::Observer* observer) override;
  const device::mojom::HidDeviceInfo* GetDeviceInfo(
      content::WebContents* web_contents,
      const std::string& guid) override;

  // permissions::ChooserContextBase::PermissionObserver:
  void OnPermissionRevoked(const url::Origin& requesting_origin,
                           const url::Origin& embedding_origin) override;

  // HidChooserContext::DeviceObserver:
  void OnDeviceAdded(const device::mojom::HidDeviceInfo&) override;
  void OnDeviceRemoved(const device::mojom::HidDeviceInfo&) override;
  void OnHidManagerConnectionError() override;
  void OnHidChooserContextShutdown() override;

 private:
  ScopedObserver<HidChooserContext,
                 HidChooserContext::DeviceObserver,
                 &HidChooserContext::AddDeviceObserver,
                 &HidChooserContext::RemoveDeviceObserver>
      device_observer_{this};
  ScopedObserver<permissions::ChooserContextBase,
                 permissions::ChooserContextBase::PermissionObserver>
      permission_observer_{this};
  base::ObserverList<content::HidDelegate::Observer> observer_list_;
};

#endif  // CHROME_BROWSER_HID_CHROME_HID_DELEGATE_H_
