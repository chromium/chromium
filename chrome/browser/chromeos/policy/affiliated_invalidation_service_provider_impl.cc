// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/affiliated_invalidation_service_provider_impl.h"

#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_identity_provider.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chrome/browser/invalidation/deprecated_profile_invalidation_provider_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/fcm_invalidation_service.h"
#include "components/invalidation/impl/fcm_network_handler.h"
#include "components/invalidation/impl/invalidation_state_tracker.h"
#include "components/invalidation/impl/invalidator_storage.h"
#include "components/invalidation/impl/per_user_topic_registration_manager.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/impl/ticl_invalidation_service.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace policy {

namespace {

// Currently InvalidationService sends TRANSIENT_INVALIDATION_ERROR in case if
// topic failing to register, but at the same time InvalidationService has retry
// logic. In case if retry is successful and all the topic succeeded to
// register, InvalidationService sends INVALIDATION_ENABLED. So
// InvalidationServiceObserver should give InvalidationService a chance to
// reregister topic, and after some time if INVALIDATION_ENABLED is not being
// received, manually check with delayed task. If after
// |kCheckInvalidatorStateDelay|, InvalidationService is still in
// TRANSIENT_INVALIDATION_ERROR state, disconnect from it and try to reregister
// all the topics.
constexpr base::TimeDelta kCheckInvalidatorStateDelay =
    base::TimeDelta::FromMinutes(3);

// After reregistering all the topics |kTransientErrorDisconnectLimit| number of
// times, when InvalidationService is failing due to
// TRANSIENT_INVALIDATION_ERROR, stop disconnecting, and let registered topics
// to keep being registered and failing topics to keep being failed. This is the
// best effort behaviour. If |kTransientErrorDisconnectLimit| is too high, at
// some point Firebase Cloud Message will start throttling register request for
// this client.
constexpr int kTransientErrorDisconnectLimit = 3;

invalidation::ProfileInvalidationProvider* GetInvalidationProvider(
    Profile* profile) {
  if (base::FeatureList::IsEnabled(features::kPolicyFcmInvalidations)) {
    return invalidation::ProfileInvalidationProviderFactory::GetForProfile(
        profile);
  }
  return invalidation::DeprecatedProfileInvalidationProviderFactory::
      GetForProfile(profile);
}

// Runs on UI thread.
void RequestProxyResolvingSocketFactoryOnUIThread(
    base::WeakPtr<invalidation::TiclInvalidationService> owner,
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  if (!owner)
    return;
  if (g_browser_process->system_network_context_manager()) {
    g_browser_process->system_network_context_manager()
        ->GetContext()
        ->CreateProxyResolvingSocketFactory(std::move(receiver));
  }
}

// Runs on IO thread.
void RequestProxyResolvingSocketFactory(
    base::WeakPtr<invalidation::TiclInvalidationService> owner,
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&RequestProxyResolvingSocketFactoryOnUIThread,
                                owner, std::move(receiver)));
}

}  // namespace

class AffiliatedInvalidationServiceProviderImpl::InvalidationServiceObserver
    : public syncer::InvalidationHandler {
 public:
  explicit InvalidationServiceObserver(
      AffiliatedInvalidationServiceProviderImpl* parent,
      invalidation::InvalidationService* invalidation_service);
  ~InvalidationServiceObserver() override;

  invalidation::InvalidationService* GetInvalidationService();
  void CheckInvalidatorState();
  bool IsServiceConnected() const;

  // public syncer::InvalidationHandler:
  void OnInvalidatorStateChange(syncer::InvalidatorState state) override;
  void OnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) override;
  std::string GetOwnerName() const override;

 private:
  AffiliatedInvalidationServiceProviderImpl* parent_;
  invalidation::InvalidationService* invalidation_service_;
  bool is_service_connected_;
  bool is_observer_ready_;
  base::OneShotTimer transient_error_retry_timer_;

  // The number of times TRANSIENT_INVALIDATION_ERROR should cause disconnect.
  int transient_error_disconnect_limit_ = kTransientErrorDisconnectLimit;

  DISALLOW_COPY_AND_ASSIGN(InvalidationServiceObserver);
};

AffiliatedInvalidationServiceProviderImpl::InvalidationServiceObserver::
    InvalidationServiceObserver(
        AffiliatedInvalidationServiceProviderImpl* parent,
        invalidation::InvalidationService* invalidation_service)
    : parent_(parent),
      invalidation_service_(invalidation_service),
      is_service_connected_(false),
      is_observer_ready_(false) {
  invalidation_service_->RegisterInvalidationHandler(this);
  is_service_connected_ = invalidation_service->GetInvalidatorState() ==
                          syncer::INVALIDATIONS_ENABLED;
  is_observer_ready_ = true;
}

AffiliatedInvalidationServiceProviderImpl::InvalidationServiceObserver::
    ~InvalidationServiceObserver() {
  is_observer_ready_ = false;
  invalidation_service_->UnregisterInvalidationHandler(this);
}

invalidation::InvalidationService*
AffiliatedInvalidationServiceProviderImpl::InvalidationServiceObserver::
    GetInvalidationService() {
  return invalidation_service_;
}

bool AffiliatedInvalidationServiceProviderImpl::InvalidationServiceObserver::
         IsServiceConnected() const {
  return is_service_connected_;
}

void AffiliatedInvalidationServiceProviderImpl::InvalidationServiceObserver::
    CheckInvalidatorState() {
  // Treat TRANSIENT_INVALIDATION_ERROR as an error, and disconnect the service
  // if connected.
  DCHECK(is_observer_ready_);
  DCHECK(invalidation_service_);
  DCHECK(parent_);

  syncer::InvalidatorState state = invalidation_service_->GetInvalidatorState();
  bool is_service_connected = (state == syncer::INVALIDATIONS_ENABLED);

  if (is_service_connected_ == is_service_connected)
    return;

  if (state == syncer::TRANSIENT_INVALIDATION_ERROR) {
    // Do not cause disconnect if the number of disconnections caused by
    // TRANSIENT_INVALIDATION_ERROR is more than the limit.
    if (!transient_error_disconnect_limit_)
      return;
    --transient_error_disconnect_limit_;
  }

  is_service_connected_ = is_service_connected;
  if (is_service_connected_)
    parent_->OnInvalidationServiceConnected(invalidation_service_);
  else
    parent_->OnInvalidationServiceDisconnected(invalidation_service_);
}

void AffiliatedInvalidationServiceProviderImpl::InvalidationServiceObserver::
    OnInvalidatorStateChange(syncer::InvalidatorState state) {
  if (!is_observer_ready_)
    return;

  // TODO(crbug/1007287): Handle 3 different states of
  // AffiliatedInvalidationServiceProvider properly.
  if (is_service_connected_) {
    // If service is connected, do NOT notify parent in case:
    //   * state == INVALIDATIONS_ENABLED
    //   * state == TRANSIENT_INVALIDATION_ERROR, hopefully will be resolved by
    //     InvalidationService, if not InvalidationService should notify again
    //     with another more severe state.
    bool should_notify = (state != syncer::INVALIDATIONS_ENABLED &&
                          state != syncer::TRANSIENT_INVALIDATION_ERROR);

    if (should_notify) {
      is_service_connected_ = false;
      parent_->OnInvalidationServiceDisconnected(invalidation_service_);
    } else if (state == syncer::TRANSIENT_INVALIDATION_ERROR) {
      transient_error_retry_timer_.Stop();
      transient_error_retry_timer_.Start(
          FROM_HERE, kCheckInvalidatorStateDelay,
          base::BindOnce(&AffiliatedInvalidationServiceProviderImpl::
                             InvalidationServiceObserver::CheckInvalidatorState,
                         base::Unretained(this)));
    }
  } else {
    // If service is disconnected, ONLY notify parent in case:
    //   * state == INVALIDATIONS_ENABLED
    bool should_notify = (state == syncer::INVALIDATIONS_ENABLED);
    if (should_notify) {
      is_service_connected_ = true;
      parent_->OnInvalidationServiceConnected(invalidation_service_);
    }
  }
}

void AffiliatedInvalidationServiceProviderImpl::InvalidationServiceObserver::
    OnIncomingInvalidation(
        const syncer::ObjectIdInvalidationMap& invalidation_map) {
}

std::string
AffiliatedInvalidationServiceProviderImpl::InvalidationServiceObserver::
    GetOwnerName() const {
  return "AffiliatedInvalidationService";
}

AffiliatedInvalidationServiceProviderImpl::
    AffiliatedInvalidationServiceProviderImpl()
    : invalidation_service_(nullptr), consumer_count_(0), is_shut_down_(false) {
  // The AffiliatedInvalidationServiceProviderImpl should be created before any
  // user Profiles.
  DCHECK(g_browser_process->profile_manager()->GetLoadedProfiles().empty());

  // Subscribe to notification about new user profiles becoming available.
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                 content::NotificationService::AllSources());
}

AffiliatedInvalidationServiceProviderImpl::
~AffiliatedInvalidationServiceProviderImpl() {
  // Verify that the provider was shut down first.
  DCHECK(is_shut_down_);
}

void AffiliatedInvalidationServiceProviderImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED, type);
  DCHECK(!is_shut_down_);
  Profile* profile = content::Details<Profile>(details).ptr();
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      GetInvalidationProvider(profile);
  if (!invalidation_provider) {
    // If the Profile does not support invalidation (e.g. guest, incognito),
    // ignore it.
    return;
  }
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user || !user->IsAffiliated()) {
    // If the Profile belongs to a user who is not affiliated on the device,
    // ignore it.
    return;
  }

  // Create a state observer for the user's invalidation service.
  invalidation::InvalidationService* invalidation_service;
  if (base::FeatureList::IsEnabled(features::kPolicyFcmInvalidations)) {
    invalidation_service =
        invalidation_provider->GetInvalidationServiceForCustomSender(
            policy::kPolicyFCMInvalidationSenderID);
  } else {
    invalidation_service = invalidation_provider->GetInvalidationService();
  }
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
  if (consumers_.HasObserver(consumer) || is_shut_down_)
    return;

  consumers_.AddObserver(consumer);
  ++consumer_count_;

  if (invalidation_service_)
    consumer->OnInvalidationServiceSet(invalidation_service_);
  else if (consumer_count_ == 1)
    FindConnectedInvalidationService();
}

void AffiliatedInvalidationServiceProviderImpl::UnregisterConsumer(
    Consumer* consumer) {
  if (!consumers_.HasObserver(consumer))
    return;

  consumers_.RemoveObserver(consumer);
  --consumer_count_;

  if (invalidation_service_ && consumer_count_ == 0) {
    invalidation_service_ = nullptr;
    DestroyDeviceInvalidationService();
  }
}

void AffiliatedInvalidationServiceProviderImpl::Shutdown() {
  is_shut_down_ = true;

  registrar_.RemoveAll();
  profile_invalidation_service_observers_.clear();
  device_invalidation_service_observer_.reset();

  if (invalidation_service_) {
    invalidation_service_ = nullptr;
    // Explicitly notify consumers that the invalidation service they were using
    // is no longer available.
    SetInvalidationService(nullptr);
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
  invalidation_service_ = nullptr;
  SetInvalidationService(invalidation_service);

  if (invalidation_service_ &&
      device_invalidation_service_ &&
      invalidation_service_ != device_invalidation_service_.get()) {
    // If a different invalidation service is being made available to consumers
    // now, destroy the device-global one.
    DestroyDeviceInvalidationService();
  }
}

void
AffiliatedInvalidationServiceProviderImpl::OnInvalidationServiceDisconnected(
    invalidation::InvalidationService* invalidation_service) {
  DCHECK(!is_shut_down_);

  if (invalidation_service != invalidation_service_) {
    // If the invalidation service which disconnected was not being made
    // available to consumers, return.
    return;
  }

  // The invalidation service which disconnected was being made available to
  // consumers. Stop making it available.
  DCHECK(consumer_count_);
  invalidation_service_ = nullptr;

  // Try to make another invalidation service available to consumers.
  FindConnectedInvalidationService();

  // If no other connected invalidation service was found, explicitly notify
  // consumers that the invalidation service they were using is no longer
  // available.
  if (!invalidation_service_)
    SetInvalidationService(nullptr);
}

void
AffiliatedInvalidationServiceProviderImpl::FindConnectedInvalidationService() {
  DCHECK(!invalidation_service_);
  DCHECK(consumer_count_);
  DCHECK(!is_shut_down_);

  for (const auto& observer : profile_invalidation_service_observers_) {
    if (observer->IsServiceConnected()) {
      // If a connected invalidation service belonging to an affiliated
      // logged-in user is found, make it available to consumers.
      DestroyDeviceInvalidationService();
      SetInvalidationService(observer->GetInvalidationService());
      return;
    }
  }

  if (!device_invalidation_service_) {
    // If no other connected invalidation service was found and no device-global
    // invalidation service exists, create one.
    device_invalidation_service_ = InitializeDeviceInvalidationService();
    device_invalidation_service_observer_.reset(
        new InvalidationServiceObserver(
                this,
                device_invalidation_service_.get()));
  }

  if (device_invalidation_service_observer_->IsServiceConnected()) {
    // If the device-global invalidation service is connected already, make it
    // available to consumers immediately. Otherwise, the invalidation service
    // will be made available to clients when it successfully connects.
    OnInvalidationServiceConnected(device_invalidation_service_.get());
  }
}

void AffiliatedInvalidationServiceProviderImpl::SetInvalidationService(
    invalidation::InvalidationService* invalidation_service) {
  DCHECK(!invalidation_service_);
  invalidation_service_ = invalidation_service;
  for (auto& observer : consumers_)
    observer.OnInvalidationServiceSet(invalidation_service_);
}

void
AffiliatedInvalidationServiceProviderImpl::DestroyDeviceInvalidationService() {
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

  device_identity_provider_ =
      std::make_unique<chromeos::DeviceIdentityProvider>(
          chromeos::DeviceOAuth2TokenServiceFactory::Get());

  if (base::FeatureList::IsEnabled(features::kPolicyFcmInvalidations)) {
    device_instance_id_driver_ =
        std::make_unique<instance_id::InstanceIDDriver>(
            g_browser_process->gcm_driver());
    DCHECK(device_instance_id_driver_);
    auto device_invalidation_service =
        std::make_unique<invalidation::FCMInvalidationService>(
            device_identity_provider_.get(),
            base::BindRepeating(&syncer::FCMNetworkHandler::Create,
                                g_browser_process->gcm_driver(),
                                device_instance_id_driver_.get()),
            base::BindRepeating(
                &syncer::PerUserTopicRegistrationManager::Create,
                device_identity_provider_.get(),
                g_browser_process->local_state(),
                base::RetainedRef(url_loader_factory)),
            device_instance_id_driver_.get(), g_browser_process->local_state(),
            policy::kPolicyFCMInvalidationSenderID);
    device_invalidation_service->Init();
    return device_invalidation_service;
  }
  auto device_invalidation_service =
      std::make_unique<invalidation::TiclInvalidationService>(
          GetUserAgent(), device_identity_provider_.get(),
          g_browser_process->gcm_driver(),
          base::BindRepeating(&RequestProxyResolvingSocketFactory),
          base::CreateSingleThreadTaskRunner({content::BrowserThread::IO}),
          std::move(url_loader_factory),
          content::GetNetworkConnectionTracker());

  device_invalidation_service->Init(
      std::make_unique<invalidation::InvalidatorStorage>(
          g_browser_process->local_state()));
  return device_invalidation_service;
}

}  // namespace policy
