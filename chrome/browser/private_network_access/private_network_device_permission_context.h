// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_PRIVATE_NETWORK_DEVICE_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_PRIVATE_NETWORK_DEVICE_PERMISSION_CONTEXT_H_

#include "base/containers/queue.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/permissions/object_permission_context_base.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "third_party/blink/public/mojom/private_network_device/private_network_device.mojom.h"
#include "url/origin.h"

class Profile;

// Manages the permissions for Private Network device objects. A Private Network
// device permission object consists of its id, name and IP address.
// The id is provided by the device in `Private-Network-Access-ID` preflight
// header. The name is provided by the device in `Private-Network-Access-Name`
// preflight header.
class PrivateNetworkDevicePermissionContext
    : public permissions::ObjectPermissionContextBase {
 public:
  explicit PrivateNetworkDevicePermissionContext(Profile* profile);

  PrivateNetworkDevicePermissionContext(
      const PrivateNetworkDevicePermissionContext&) = delete;
  PrivateNetworkDevicePermissionContext& operator=(
      const PrivateNetworkDevicePermissionContext&) = delete;

  ~PrivateNetworkDevicePermissionContext() override;

  std::string GetKeyForObject(const base::Value::Dict& object) override;
  std::u16string GetObjectDisplayName(const base::Value::Dict& object) override;

  bool IsValidObject(const base::Value::Dict& object) override;

  // Grants `origin` access to the device.
  void GrantDevicePermission(const url::Origin& origin,
                             const blink::mojom::PrivateNetworkDevice& device);

  // Checks if `origin` has access to `device`.
  bool HasDevicePermission(const url::Origin& origin,
                           const blink::mojom::PrivateNetworkDevice& device);

  // Tracks the set of devices to which an origin has temporary access to.
  std::map<url::Origin, std::set<net::IPAddress>> ephemeral_devices_;

  base::WeakPtr<PrivateNetworkDevicePermissionContext> AsWeakPtr();

  static base::Value::Dict DeviceInfoToValue(
      const blink::mojom::PrivateNetworkDevice& device);

  base::WeakPtrFactory<PrivateNetworkDevicePermissionContext> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_PRIVATE_NETWORK_DEVICE_PERMISSION_CONTEXT_H_
