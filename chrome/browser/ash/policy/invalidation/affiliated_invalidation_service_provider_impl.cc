// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/invalidation/affiliated_invalidation_service_provider_impl.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_identity/device_identity_provider.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/fcm_invalidation_service.h"
#include "components/invalidation/impl/fcm_network_handler.h"
#include "components/invalidation/impl/per_user_topic_subscription_manager.h"
#include "components/invalidation/invalidation_factory.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/user_manager/user.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

invalidation::ProfileInvalidationProvider* GetInvalidationProvider(
    Profile* profile) {
  return invalidation::ProfileInvalidationProviderFactory::GetForProfile(
      profile);
}

}  // namespace

class AffiliatedInvalidationServiceProviderImpl::InvalidationServiceObserver
    : public invalidation::InvalidationHandler {
 public:
  explicit InvalidationServiceObserver(
      AffiliatedInvalidationServiceProviderImpl* parent,
      invalidation::InvalidationService* invalidation_service);

  InvalidationServiceObserver(const InvalidationServiceObserver&) = delete;
  InvalidationServiceObserver& operator=(const InvalidationServiceObserver&) =
      delete;

  ~InvalidationServiceObserver() override;

  invalidation::InvalidationService* GetInvalidationService();
  bool IsServiceConnected() const;

  // public invalidation::InvalidationHandler:
  void OnInvalidatorStateChange(invalidation::InvalidatorState state) override;
  void OnIncomingInvalidation(
      const invalidation::Invalidation& invalidation) override;
  std::string GetOwnerName() const override;

 private:
  raw_ptr<AffiliatedInvalidationServiceProviderImpl> parent_;
  const raw_ptr<invalidation::InvalidationService> invalidation_service_;
  bool is_service_connected_;
  bool is_observer_ready_;

  base::ScopedObservation<invalidation::InvalidationService,
                          invalidation::InvalidationHandler>
      invalidation_service_observation_{this};
};

AffiliatedInvalidationServiceProviderImpl::InvalidationServiceObserver::
    InvalidationServiceObserver(
        AffiliatedInvalidationServiceProviderImpl* parent,
        invalidation::InvalidationService* invalidation_service)
    : parent_(parent),
      invalidation_service_(invalidation_service),
      is_service_connected_(false),
      is_observer_ready_(false) {
  DCHECK(invalidation_service_);
  invalidation_service_observation_.Observe(invalidation_service_);
  is_service_connected_ = invalidation_service->GetInvalidatorState() ==
                          invalidation::InvalidatorState::kEnabled;
  is_observer_ready_ = true;
}

AffiliatedInvalidationServiceProviderImpl::InvalidationServiceObserver::
    ~InvalidationServiceObserver() {
  is_observer_ready_ = false;
}

invalidation::InvalidationService* AffiliatedInvalidationServiceProviderImpl::
    InvalidationServiceObserver::GetInvalidationService() {
  return invalidation_service_;
}

bool AffiliatedInvalidationServiceProviderImpl::InvalidationServiceObserver::
    IsServiceConnected() const {
  return is_service_connected_;
}

void AffiliatedInvalidationServiceProviderImpl::InvalidationServiceObserver::
    OnInvalidatorStateChange(invalidation::InvalidatorState state) {
  if (!is_observer_ready_) {
    return;
  }

  const bool new_is_service_connected =
      (state == invalidation::InvalidatorState::kEnabled);

  if (is_service_connected_ == new_is_service_connected) {
    return;
  }

  is_service_connected_ = new_is_service_connected;
  if (is_service_connected_) {
    parent_->OnInvalidationServiceConnected(invalidation_service_);
  } else {
    parent_->OnInvalidationServiceDisconnected(invalidation_service_);
  }
}

void AffiliatedInvalidationServiceProviderImpl::InvalidationServiceObserver::
    OnIncomingInvalidation(const invalidation::Invalidation& invalidation) {}

std::string AffiliatedInvalidationServiceProviderImpl::
    InvalidationServiceObserver::GetOwnerName() const {
  return "AffiliatedInvalidationService";
}

AffiliatedInvalidationServiceProviderImpl::
    AffiliatedInvalidationServiceProviderImpl()
    : current_invalidation_service_(nullptr),
      consumer_count_(0),
      is_shut_down_(false) {
  // The AffiliatedInvalidationServiceProviderImpl should be created before any
  // user Profiles.
  DCHECK(g_browser_process->profile_manager()->GetLoadedProfiles().empty());

  // Subscribe to notification about new user profiles becoming available.
  session_observation_.Observe(session_manager::SessionManager::Get());
}

AffiliatedInvalidationServiceProviderImpl::
    ~AffiliatedInvalidationServiceProviderImpl() {
  // Verify that the provider was shut down first.
  DCHECK(is_shut_down_);
}

void AffiliatedInvalidationServiceProviderImpl::OnUserProfileLoaded(
    const AccountId& account_id) {
  DCHECK(!is_shut_down_);
  Profile* profile =
      ash::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      GetInvalidationProvider(profile);
  if (!invalidation_provider) {
    // If the Profile does not support invalidation (e.g. guest, incognito),
    // ignore it.
    return;
  }
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user || !user->IsAffiliated()) {
    // If the Profile belongs to a user who is not affiliated on the device,
    // ignore it.
    return;
  }

  // Create a state observer for the user's invalidation service.
  auto invalidation_service_or_listener =
      invalidation_provider->GetInvalidationServiceOrListener(
          policy::kPolicyFCMInvalidationSenderID,
          invalidation::InvalidationListener::kProjectNumberEnterprise);
  CHECK(std::holds_alternative<invalidation::InvalidationService*>(
      invalidation_service_or_listener))
      << "AffiliatedInvalidationServiceProviderImpl is created with "
         "InvalidationListener setup";
  auto* invalidation_service = std::get<invalidation::InvalidationService*>(
      invalidation_service_or_listener);
  profile_invalidation_service_observers_.push_back(
      std::make_unique<InvalidationServiceObserver>(this,
                                                    invalidation_service));
  if (profile_invalidation_service_observers_.back()->IsServiceConnected()) {
    // If the invalidation service is connected, check whether to switch to it.
    OnInvalidationServiceConnected(invalidation_service);
  }
}

void AffiliatedInvalidationServiceProviderImpl::RegisterConsumer(
    Consumer* consumer) {
  if (consumers_.HasObserver(consumer) || is_shut_down_) {
    return;
  }

  consumers_.AddObserver(consumer);
  ++consumer_count_;

  if (current_invalidation_service_) {
    consumer->OnInvalidationServiceSet(current_invalidation_service_);
  } else if (consumer_count_ == 1) {
    FindConnectedInvalidationService();
  }
}

void AffiliatedInvalidationServiceProviderImpl::UnregisterConsumer(
    Consumer* consumer) {
  if (!consumers_.HasObserver(consumer)) {
    return;
  }

  consumers_.RemoveObserver(consumer);
  --consumer_count_;

  if (current_invalidation_service_ && consumer_count_ == 0) {
    current_invalidation_service_ = nullptr;
    DestroyDeviceInvalidationService();
  }
}

void AffiliatedInvalidationServiceProviderImpl::Shutdown() {
  is_shut_down_ = true;

  session_observation_.Reset();
  profile_invalidation_service_observers_.clear();
  device_invalidation_service_observer_.reset();

  if (current_invalidation_service_) {
    current_invalidation_service_ = nullptr;
    // Explicitly notify consumers that the invalidation service they were using
    // is no longer available.
    SetCurrentInvalidationService(nullptr);
  }

  DestroyDeviceInvalidationService();
}

invalidation::InvalidationService*
AffiliatedInvalidationServiceProviderImpl::GetDeviceInvalidationServiceForTest()
    const {
  return device_invalidation_service_.get();
}

void AffiliatedInvalidationServiceProviderImpl::OnInvalidationServiceConnected(
    invalidation::InvalidationService* invalidation_service) {
  DCHECK(!is_shut_down_);

  if (consumer_count_ == 0) {
    // If there are no consumers, no invalidation service is required.
    return;
  }

  if (!device_invalidation_service_) {
    // The lack of a device-global invalidation service implies that another
    // connected invalidation service is being made available to consumers
    // already. There is no need to switch from that to the service which just
    // connected.
    return;
  }

  // Make the invalidation service that just connected available to consumers.
  current_invalidation_service_ = nullptr;
  SetCurrentInvalidationService(invalidation_service);

  if (current_invalidation_service_ && device_invalidation_service_ &&
      current_invalidation_service_ != device_invalidation_service_.get()) {
    // If a different invalidation service is being made available to consumers
    // now, destroy the device-global one.
    DestroyDeviceInvalidationService();
  }
}

void AffiliatedInvalidationServiceProviderImpl::
    OnInvalidationServiceDisconnected(
        invalidation::InvalidationService* invalidation_service) {
  DCHECK(!is_shut_down_);

  if (invalidation_service != current_invalidation_service_) {
    // If the invalidation service which disconnected was not being made
    // available to consumers, return.
    return;
  }

  // The invalidation service which disconnected was being made available to
  // consumers. Stop making it available.
  DCHECK(consumer_count_);
  current_invalidation_service_ = nullptr;

  // Try to make another invalidation service available to consumers.
  FindConnectedInvalidationService();

  // If no other connected invalidation service was found, explicitly notify
  // consumers that the invalidation service they were using is no longer
  // available.
  if (!current_invalidation_service_) {
    SetCurrentInvalidationService(nullptr);
  }
}

void AffiliatedInvalidationServiceProviderImpl::
    FindConnectedInvalidationService() {
  DCHECK(!current_invalidation_service_);
  DCHECK(consumer_count_);
  DCHECK(!is_shut_down_);

  for (const auto& observer : profile_invalidation_service_observers_) {
    if (observer->IsServiceConnected()) {
      // If a connected invalidation service belonging to an affiliated
      // logged-in user is found, make it available to consumers.
      DestroyDeviceInvalidationService();
      SetCurrentInvalidationService(observer->GetInvalidationService());
      return;
    }
  }

  if (!device_invalidation_service_) {
    // If no other connected invalidation service was found and no device-global
    // invalidation service exists, create one.
    device_invalidation_service_ = InitializeDeviceInvalidationService();
    device_invalidation_service_observer_ =
        std::make_unique<InvalidationServiceObserver>(
            this, device_invalidation_service_.get());
  }

  if (device_invalidation_service_observer_->IsServiceConnected()) {
    // If the device-global invalidation service is connected already, make it
    // available to consumers immediately. Otherwise, the invalidation service
    // will be made available to clients when it successfully connects.
    OnInvalidationServiceConnected(device_invalidation_service_.get());
  }
}

void AffiliatedInvalidationServiceProviderImpl::SetCurrentInvalidationService(
    invalidation::InvalidationService* invalidation_service) {
  DCHECK(!current_invalidation_service_);
  current_invalidation_service_ = invalidation_service;
  for (auto& observer : consumers_) {
    observer.OnInvalidationServiceSet(current_invalidation_service_);
  }
}

void AffiliatedInvalidationServiceProviderImpl::
    DestroyDeviceInvalidationService() {
  device_invalidation_service_observer_.reset();
  device_invalidation_service_.reset();
  device_identity_provider_.reset();
  device_instance_id_driver_.reset();
}

std::unique_ptr<invalidation::InvalidationService>
AffiliatedInvalidationServiceProviderImpl::
    InitializeDeviceInvalidationService() {
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  if (g_browser_process->system_network_context_manager()) {
    // system_network_context_manager() can be null during unit tests.
    url_loader_factory = g_browser_process->system_network_context_manager()
                             ->GetSharedURLLoaderFactory();
    DCHECK(url_loader_factory);
  }

  device_identity_provider_ = std::make_unique<DeviceIdentityProvider>(
      DeviceOAuth2TokenServiceFactory::Get());

  device_instance_id_driver_ = std::make_unique<instance_id::InstanceIDDriver>(
      g_browser_process->gcm_driver());

  DCHECK(device_instance_id_driver_);
  auto invalidation_service_or_listener =
      invalidation::CreateInvalidationServiceOrListener(
          device_identity_provider_.get(), g_browser_process->gcm_driver(),
          device_instance_id_driver_.get(), url_loader_factory,
          g_browser_process->local_state(), kPolicyFCMInvalidationSenderID,
          /*project_number=*/"", /*log_prefix=*/"");
  CHECK(std::holds_alternative<
        std::unique_ptr<invalidation::InvalidationService>>(
      invalidation_service_or_listener))
      << "AffiliatedInvalidationServiceProviderImpl is created with "
         "InvalidationListener setup";
  return std::move(std::get<std::unique_ptr<invalidation::InvalidationService>>(
      invalidation_service_or_listener));
}

}  // namespace policy
