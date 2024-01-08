// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_INVALIDATION_AFFILIATED_INVALIDATION_SERVICE_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_INVALIDATION_AFFILIATED_INVALIDATION_SERVICE_PROVIDER_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/policy/invalidation/affiliated_invalidation_service_provider.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

class AccountId;
namespace invalidation {
class InvalidationService;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace policy {

class AffiliatedInvalidationServiceProviderImpl
    : public AffiliatedInvalidationServiceProvider,
      public session_manager::SessionManagerObserver {
 public:
  AffiliatedInvalidationServiceProviderImpl();

  AffiliatedInvalidationServiceProviderImpl(
      const AffiliatedInvalidationServiceProviderImpl&) = delete;
  AffiliatedInvalidationServiceProviderImpl& operator=(
      const AffiliatedInvalidationServiceProviderImpl&) = delete;

  ~AffiliatedInvalidationServiceProviderImpl() override;

  // session_manager::SessionManagerObserver:
  void OnUserProfileLoaded(const AccountId& account_id) override;

  // AffiliatedInvalidationServiceProvider:
  void RegisterConsumer(Consumer* consumer) override;
  void UnregisterConsumer(Consumer* consumer) override;
  void Shutdown() override;

  invalidation::InvalidationService* GetDeviceInvalidationServiceForTest()
      const;

 private:
  // Helper that monitors the status of a single |InvalidationService|.
  class InvalidationServiceObserver;

  // Status updates received from |InvalidationServiceObserver|s.
  void OnInvalidationServiceConnected(
      invalidation::InvalidationService* invalidation_service);
  void OnInvalidationServiceDisconnected(
      invalidation::InvalidationService* invalidation_service);

  // Checks whether a connected |InvalidationService| affiliated with the
  // device's enrollment domain is available. If so, notifies the consumers.
  // Otherwise, consumers will be notified once such an invalidation service
  // becomes available.
  // Further ensures that a device-global invalidation service is running iff
  // there is no other connected service available for use and there is at least
  // one registered consumer.
  void FindConnectedInvalidationService();

  // Choose |invalidation_service| as the |current_invalidation_service_| and
  // notify consumers.
  void SetCurrentInvalidationService(
      invalidation::InvalidationService* invalidation_service);

  // Destroy the device-global invalidation service, if any.
  void DestroyDeviceInvalidationService();

  // Initializes and returns an `InvalidationService`.
  std::unique_ptr<invalidation::InvalidationService>
  InitializeDeviceInvalidationService();

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  // State observer for the device-global invalidation service.
  std::unique_ptr<InvalidationServiceObserver>
      device_invalidation_service_observer_;

  // The |device_identity_provider_| must be declared before
  // |device_invalidation_service_| because the service has a pointer to it.
  std::unique_ptr<invalidation::IdentityProvider> device_identity_provider_;

  // The |device_instance_id_driver_| must be declared before
  // |device_invalidation_service_| because the service has a pointer to it. Not
  // null only when FCM is enabled.
  std::unique_ptr<instance_id::InstanceIDDriver> device_instance_id_driver_;

  // Device-global invalidation service.
  std::unique_ptr<invalidation::InvalidationService>
      device_invalidation_service_;

  // State observers for logged-in users' invalidation services.
  std::vector<std::unique_ptr<InvalidationServiceObserver>>
      profile_invalidation_service_observers_;

  // The invalidation service currently used by consumers. nullptr if there are
  // no registered consumers or no connected invalidation service is available
  // for use.
  raw_ptr<invalidation::InvalidationService> current_invalidation_service_;

  base::ObserverList<Consumer, true>::Unchecked consumers_;
  int consumer_count_;

  bool is_shut_down_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_INVALIDATION_AFFILIATED_INVALIDATION_SERVICE_PROVIDER_IMPL_H_
