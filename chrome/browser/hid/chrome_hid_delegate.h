// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_CHROME_HID_DELEGATE_H_
#define CHROME_BROWSER_HID_CHROME_HID_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "components/permissions/object_permission_context_base.h"
#include "content/public/browser/hid_delegate.h"

class ChromeHidDelegate
    : public content::HidDelegate,
      public permissions::ObjectPermissionContextBase::PermissionObserver,
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
  bool CanRequestDevicePermission(content::WebContents* web_contents) override;
  bool HasDevicePermission(content::WebContents* web_contents,
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

  // permissions::ObjectPermissionContextBase::PermissionObserver:
  void OnPermissionRevoked(const url::Origin& origin) override;

  // HidChooserContext::DeviceObserver:
  void OnDeviceAdded(const device::mojom::HidDeviceInfo&) override;
  void OnDeviceRemoved(const device::mojom::HidDeviceInfo&) override;
  void OnDeviceChanged(const device::mojom::HidDeviceInfo&) override;
  void OnHidManagerConnectionError() override;
  void OnHidChooserContextShutdown() override;

 private:
  base::ScopedObservation<HidChooserContext,
                          HidChooserContext::DeviceObserver,
                          &HidChooserContext::AddDeviceObserver,
                          &HidChooserContext::RemoveDeviceObserver>
      device_observation_{this};
  base::ScopedObservation<
      permissions::ObjectPermissionContextBase,
      permissions::ObjectPermissionContextBase::PermissionObserver>
      permission_observation_{this};
  base::ObserverList<content::HidDelegate::Observer> observer_list_;
};

#endif  // CHROME_BROWSER_HID_CHROME_HID_DELEGATE_H_
