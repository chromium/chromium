// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_CHOOSER_CONTEXT_H_
#define CHROME_BROWSER_HID_HID_CHOOSER_CONTEXT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/unguessable_token.h"
#include "components/permissions/chooser_context_base.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "url/origin.h"

class Profile;

namespace base {
class Value;
}

// Manages the internal state and connection to the device service for the
// Human Interface Device (HID) chooser UI.
class HidChooserContext : public permissions::ChooserContextBase,
                          public device::mojom::HidManagerClient {
 public:
  explicit HidChooserContext(Profile* profile);
  HidChooserContext(const HidChooserContext&) = delete;
  HidChooserContext& operator=(const HidChooserContext&) = delete;
  ~HidChooserContext() override;

  // This observer can be used to be notified when HID devices are connected or
  // disconnected.
  class DeviceObserver : public base::CheckedObserver {
   public:
    virtual void OnDeviceAdded(const device::mojom::HidDeviceInfo&);
    virtual void OnDeviceRemoved(const device::mojom::HidDeviceInfo&);
    virtual void OnHidManagerConnectionError();

    // Called when the HidChooserContext is shutting down. Observers must remove
    // themselves before returning.
    virtual void OnHidChooserContextShutdown() = 0;
  };

  // permissions::ChooserContextBase implementation:
  bool IsValidObject(const base::Value& object) override;
  // In addition these methods from ChooserContextBase are overridden in order
  // to expose ephemeral devices through the public interface.
  std::vector<std::unique_ptr<Object>> GetGrantedObjects(
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) override;
  std::vector<std::unique_ptr<Object>> GetAllGrantedObjects() override;
  void RevokeObjectPermission(const url::Origin& requesting_origin,
                              const url::Origin& embedding_origin,
                              const base::Value& object) override;
  base::string16 GetObjectDisplayName(const base::Value& object) override;

  // HID-specific interface for granting and checking permissions.
  void GrantDevicePermission(const url::Origin& requesting_origin,
                             const url::Origin& embedding_origin,
                             const device::mojom::HidDeviceInfo& device);
  bool HasDevicePermission(const url::Origin& requesting_origin,
                           const url::Origin& embedding_origin,
                           const device::mojom::HidDeviceInfo& device);

  // For ScopedObserver.
  void AddDeviceObserver(DeviceObserver* observer);
  void RemoveDeviceObserver(DeviceObserver* observer);

  // Forward HidManager::GetDevices.
  void GetDevices(device::mojom::HidManager::GetDevicesCallback callback);

  // Only call this if you're sure |devices_| has been initialized before-hand.
  // The returned raw pointer is owned by |devices_| and will be destroyed when
  // the device is removed.
  const device::mojom::HidDeviceInfo* GetDeviceInfo(const std::string& guid);

  device::mojom::HidManager* GetHidManager();

  // Sets |manager| as the HidManager and registers this context as a
  // HidManagerClient. Calls |callback| with the set of enumerated devices once
  // the client is registered and the initial enumeration is complete.
  void SetHidManagerForTesting(
      mojo::PendingRemote<device::mojom::HidManager> manager,
      device::mojom::HidManager::GetDevicesCallback callback);

  base::WeakPtr<HidChooserContext> AsWeakPtr();

 private:
  // device::mojom::HidManagerClient implementation:
  void DeviceAdded(device::mojom::HidDeviceInfoPtr device_info) override;
  void DeviceRemoved(device::mojom::HidDeviceInfoPtr device_info) override;

  void EnsureHidManagerConnection();
  void SetUpHidManagerConnection(
      mojo::PendingRemote<device::mojom::HidManager> manager);
  void InitDeviceList(std::vector<device::mojom::HidDeviceInfoPtr> devices);
  void OnHidManagerConnectionError();

  const bool is_incognito_;
  bool is_initialized_ = false;
  base::queue<device::mojom::HidManager::GetDevicesCallback>
      pending_get_devices_requests_;

  // Tracks the set of devices to which an origin (potentially embedded in
  // another origin) has access to. Key is (requesting_origin,
  // embedding_origin).
  std::map<std::pair<url::Origin, url::Origin>, std::set<std::string>>
      ephemeral_devices_;

  // Map from device GUID to device info.
  std::map<std::string, device::mojom::HidDeviceInfoPtr> devices_;

  mojo::Remote<device::mojom::HidManager> hid_manager_;
  mojo::AssociatedReceiver<device::mojom::HidManagerClient> client_receiver_{
      this};
  base::ObserverList<DeviceObserver> device_observer_list_;

  base::WeakPtrFactory<HidChooserContext> weak_factory_{this};
};

#endif  // CHROME_BROWSER_HID_HID_CHOOSER_CONTEXT_H_
