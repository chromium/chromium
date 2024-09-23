// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERIAL_SERIAL_CHOOSER_CONTEXT_H_
#define CHROME_BROWSER_SERIAL_SERIAL_CHOOSER_CONTEXT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "base/unguessable_token.h"
#include "components/permissions/object_permission_context_base.h"
#include "content/public/browser/serial_delegate.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/serial.mojom-forward.h"
#include "third_party/blink/public/mojom/serial/serial.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

class Profile;

namespace base {
class Value;
}

class SerialChooserContext
    : public permissions::ObjectPermissionContextBase,
      public permissions::ObjectPermissionContextBase::PermissionObserver,
      public device::mojom::SerialPortManagerClient {
 public:
  using PortObserver = content::SerialDelegate::Observer;

  explicit SerialChooserContext(Profile* profile);

  SerialChooserContext(const SerialChooserContext&) = delete;
  SerialChooserContext& operator=(const SerialChooserContext&) = delete;

  ~SerialChooserContext() override;

  static base::Value::Dict PortInfoToValue(
      const device::mojom::SerialPortInfo& port);

  // ObjectPermissionContextBase:
  std::string GetKeyForObject(const base::Value::Dict& object) override;
  bool IsValidObject(const base::Value::Dict& object) override;
  std::u16string GetObjectDisplayName(const base::Value::Dict& object) override;
  // ObjectPermissionContextBase::PermissionObserver:
  void OnPermissionRevoked(const url::Origin& origin) override;

  // In addition these methods from ObjectPermissionContextBase are overridden
  // in order to expose ephemeral devices through the public interface.
  std::vector<std::unique_ptr<Object>> GetGrantedObjects(
      const url::Origin& origin) override;
  std::vector<std::unique_ptr<Object>> GetAllGrantedObjects() override;
  void RevokeObjectPermission(const url::Origin& origin,
                              const base::Value::Dict& object) override;

  // Serial-specific interface for granting, checking, and revoking permissions.
  void GrantPortPermission(const url::Origin& origin,
                           const device::mojom::SerialPortInfo& port);
  bool HasPortPermission(const url::Origin& origin,
                         const device::mojom::SerialPortInfo& port);
  void RevokePortPermissionWebInitiated(const url::Origin& origin,
                                        const base::UnguessableToken& token);
  static bool CanStorePersistentEntry(
      const device::mojom::SerialPortInfo& port);

  // Only call this if you're sure |port_info_| has been initialized
  // before-hand. The returned raw pointer is owned by |port_info_| and will be
  // destroyed when the port is removed.
  const device::mojom::SerialPortInfo* GetPortInfo(
      const base::UnguessableToken& token);

  device::mojom::SerialPortManager* GetPortManager();

  void AddPortObserver(PortObserver* observer);
  void RemovePortObserver(PortObserver* observer);

  void SetPortManagerForTesting(
      mojo::PendingRemote<device::mojom::SerialPortManager> manager);
  void FlushPortManagerConnectionForTesting();
  base::WeakPtr<SerialChooserContext> AsWeakPtr();

  // SerialPortManagerClient implementation.
  void OnPortAdded(device::mojom::SerialPortInfoPtr port) override;
  void OnPortRemoved(device::mojom::SerialPortInfoPtr port) override;
  void OnPortConnectedStateChanged(
      device::mojom::SerialPortInfoPtr port) override;

  // KeyedService:
  void Shutdown() override;

  Profile* profile() { return profile_.get(); }

 private:
  void EnsurePortManagerConnection();
  void SetUpPortManagerConnection(
      mojo::PendingRemote<device::mojom::SerialPortManager> manager);
  void OnGetDevices(std::vector<device::mojom::SerialPortInfoPtr> ports);
  void OnPortManagerConnectionError();
  bool CanApplyPortSpecificPolicy();

  void RevokeObjectPermissionInternal(const url::Origin& origin,
                                      const base::Value::Dict& object,
                                      bool revoked_by_website);

  // This raw pointer is safe because instances of this class are created by
  // SerialChooserContextFactory as KeyedServices that will be destroyed when
  // the Profile object is destroyed.
  const raw_ptr<Profile> profile_;

  bool is_initialized_ = false;

  // Tracks the set of ports to which an origin has access to.
  std::map<url::Origin, std::set<base::UnguessableToken>> ephemeral_ports_;

  // Map from port token to port info.
  std::map<base::UnguessableToken, device::mojom::SerialPortInfoPtr> port_info_;

  mojo::Remote<device::mojom::SerialPortManager> port_manager_;
  mojo::Receiver<device::mojom::SerialPortManagerClient> client_receiver_{this};
  base::ObserverList<PortObserver> port_observer_list_;

  base::ScopedObservation<
      permissions::ObjectPermissionContextBase,
      permissions::ObjectPermissionContextBase::PermissionObserver>
      permission_observation_{this};

  base::WeakPtrFactory<SerialChooserContext> weak_factory_{this};
};

namespace base {

template <>
struct ScopedObservationTraits<SerialChooserContext,
                               SerialChooserContext::PortObserver> {
  static void AddObserver(SerialChooserContext* source,
                          SerialChooserContext::PortObserver* observer) {
    source->AddPortObserver(observer);
  }
  static void RemoveObserver(SerialChooserContext* source,
                             SerialChooserContext::PortObserver* observer) {
    source->RemovePortObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_SERIAL_SERIAL_CHOOSER_CONTEXT_H_
