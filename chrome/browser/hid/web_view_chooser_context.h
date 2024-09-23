// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_WEB_VIEW_CHOOSER_CONTEXT_H_
#define CHROME_BROWSER_HID_WEB_VIEW_CHOOSER_CONTEXT_H_

#include <map>
#include <string>

#include "base/scoped_observation.h"
#include "components/permissions/object_permission_context_base.h"
#include "services/device/public/mojom/hid.mojom.h"

class HidChooserContext;

// WebViewChooserContext stores the HID permissions for embedded WebViews.
// Permissions stored by WebViewChooserContext are ephemeral and not stored on
// disk.
//
// WebView permissions should be stored separately from other permissions for
// security reasons, e.g., crbug/1462709.
class WebViewChooserContext
    : public permissions::ObjectPermissionContextBase::PermissionObserver {
 public:
  explicit WebViewChooserContext(HidChooserContext* chooser_context);
  ~WebViewChooserContext() override;

  void GrantDevicePermission(const url::Origin& origin,
                             const url::Origin& embedding_origin,
                             const device::mojom::HidDeviceInfo& device);

  bool HasDevicePermission(const url::Origin& origin,
                           const url::Origin& embedding_origin,
                           const device::mojom::HidDeviceInfo& device) const;

  void RevokeDevicePermission(const url::Origin& origin,
                              const url::Origin& embedding_origin,
                              const device::mojom::HidDeviceInfo& device);

  // permissions::ObjectPermissionContextBase::PermissionObserver:
  void OnPermissionRevoked(const url::Origin& origin) override;

  void OnHidChooserContextShutdown();

 private:
  // Stores embedded origins that have access to device, per device.
  using DeviceMap = std::map<std::string, std::set<url::Origin>>;

  // Stores device permissions per embedding origin.
  std::map<url::Origin, DeviceMap> device_access_;

  // It is safe to store a raw pointer since `chooser_context_` owns this
  // object.
  raw_ptr<HidChooserContext> chooser_context_;

  base::ScopedObservation<
      permissions::ObjectPermissionContextBase,
      permissions::ObjectPermissionContextBase::PermissionObserver>
      permission_observation_{this};
};

#endif  // CHROME_BROWSER_HID_WEB_VIEW_CHOOSER_CONTEXT_H_
