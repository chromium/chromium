// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_CHOOSER_CONTEXT_H_
#define CHROME_BROWSER_HID_HID_CHOOSER_CONTEXT_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "chrome/browser/hid/web_view_chooser_context.h"
#include "components/permissions/object_permission_context_base.h"
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
class HidChooserContext : public permissions::ObjectPermissionContextBase,
                          public device::mojom::HidManagerClient {
 public:
  // This observer can be used to be notified when HID devices are connected or
  // disconnected.
  class DeviceObserver : public base::CheckedObserver {
   public:
    virtual void OnDeviceAdded(const device::mojom::HidDeviceInfo&);
    virtual void OnDeviceRemoved(const device::mojom::HidDeviceInfo&);
    virtual void OnDeviceChanged(const device::mojom::HidDeviceInfo&);
    virtual void OnHidManagerConnectionError();

    // Called when the HidChooserContext is shutting down. Observers must remove
    // themselves before returning.
    virtual void OnHidChooserContextShutdown() = 0;
  };

  explicit HidChooserContext(Profile* profile);
  HidChooserContext(const HidChooserContext&) = delete;
  HidChooserContext& operator=(const HidChooserContext&) = delete;
  ~HidChooserContext() override;

  static base::Value::Dict DeviceInfoToValue(
      const device::mojom::HidDeviceInfo& device);

  // Returns a human-readable string identifier for |device|.
  static std::u16string DisplayNameFromDeviceInfo(
      const device::mojom::HidDeviceInfo& device);

  // Returns true if a persistent permission can be granted for |device|.
  static bool CanStorePersistentEntry(
      const device::mojom::HidDeviceInfo& device);

  // permissions::ObjectPermissionContextBase implementation:
  std::string GetKeyForObject(const base::Value::Dict& object) override;
  bool IsValidObject(const base::Value::Dict& object) override;
  // In addition these methods from ObjectPermissionContextBase are overridden
  // in order to expose ephemeral devices through the public interface.
  std::vector<std::unique_ptr<Object>> GetGrantedObjects(
      const url::Origin& origin) override;
  std::vector<std::unique_ptr<Object>> GetAllGrantedObjects() override;
  void RevokeObjectPermission(const url::Origin& origin,
                              const base::Value::Dict& object) override;
  std::u16string GetObjectDisplayName(const base::Value::Dict& object) override;

  // HID-specific interface for granting, revoking and checking permissions.
  void GrantDevicePermission(const url::Origin& origin,
                             const device::mojom::HidDeviceInfo& device,
                             const std::optional<url::Origin>&
                                 embedding_origin_of_web_view = std::nullopt);
  void RevokeDevicePermission(const url::Origin& origin,
                              const device::mojom::HidDeviceInfo& device,
                              const std::optional<url::Origin>&
                                  embedding_origin_of_web_view = std::nullopt);
  bool HasDevicePermission(const url::Origin& origin,
                           const device::mojom::HidDeviceInfo& device,
                           const std::optional<url::Origin>&
                               embedding_origin_of_web_view = std::nullopt);

  // Returns true if `origin` is allowed to access FIDO reports.
  bool IsFidoAllowedForOrigin(const url::Origin& origin);

  // For ScopedObservation, see ScopedObservationTraits below.
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

  void PermissionForWebViewChanged();
  void PermissionForWebViewRevoked(const url::Origin& web_view_origin);

  base::WeakPtr<HidChooserContext> AsWeakPtr();

  // KeyedService:
  void Shutdown() override;

 private:
  // device::mojom::HidManagerClient implementation:
  void DeviceAdded(device::mojom::HidDeviceInfoPtr device_info) override;
  void DeviceRemoved(device::mojom::HidDeviceInfoPtr device_info) override;
  void DeviceChanged(device::mojom::HidDeviceInfoPtr device_info) override;

  void EnsureHidManagerConnection();
  void SetUpHidManagerConnection(
      mojo::PendingRemote<device::mojom::HidManager> manager);
  void InitDeviceList(std::vector<device::mojom::HidDeviceInfoPtr> devices);
  void OnHidManagerInitializedForTesting(
      device::mojom::HidManager::GetDevicesCallback callback,
      std::vector<device::mojom::HidDeviceInfoPtr> devices);
  void OnHidManagerConnectionError();
  bool CanApplyPolicy();

  // HID-specific interface for revoking device permissions.
  void RevokePersistentDevicePermission(
      const url::Origin& origin,
      const device::mojom::HidDeviceInfo& device);
  void RevokeEphemeralDevicePermission(
      const url::Origin& origin,
      const device::mojom::HidDeviceInfo& device);

  // This raw pointer is safe because instances of this class are created by
  // HidChooserContextFactory as KeyedServices that will be destroyed when the
  // Profile object is destroyed.
  const raw_ptr<Profile> profile_;

  bool is_initialized_ = false;
  base::queue<device::mojom::HidManager::GetDevicesCallback>
      pending_get_devices_requests_;

  // Tracks the set of devices to which an origin has access to.
  std::map<url::Origin, std::set<std::string>> ephemeral_devices_;

  // Map from device GUID to device info.
  std::map<std::string, device::mojom::HidDeviceInfoPtr> devices_;

  WebViewChooserContext web_view_chooser_context_{this};

  mojo::Remote<device::mojom::HidManager> hid_manager_;
  mojo::AssociatedReceiver<device::mojom::HidManagerClient> client_receiver_{
      this};
  base::ObserverList<DeviceObserver> device_observer_list_;

  base::WeakPtrFactory<HidChooserContext> weak_factory_{this};
};

namespace base {

template <>
struct ScopedObservationTraits<HidChooserContext,
                               HidChooserContext::DeviceObserver> {
  static void AddObserver(HidChooserContext* source,
                          HidChooserContext::DeviceObserver* observer) {
    source->AddDeviceObserver(observer);
  }
  static void RemoveObserver(HidChooserContext* source,
                             HidChooserContext::DeviceObserver* observer) {
    source->RemoveDeviceObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_HID_HID_CHOOSER_CONTEXT_H_
