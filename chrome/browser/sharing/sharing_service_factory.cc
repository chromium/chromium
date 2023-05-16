// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_service_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/click_to_call/phone_number_regex.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_device_registration.h"
#include "chrome/browser/sharing/sharing_device_source_sync.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_handler_registry_impl.h"
#include "chrome/browser/sharing/sharing_message_bridge.h"
#include "chrome/browser/sharing/sharing_message_bridge_factory.h"
#include "chrome/browser/sharing/sharing_message_sender.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "chrome/browser/sharing/web_push/web_push_sender.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/buildflags.h"
#include "components/gcm_driver/crypto/gcm_encryption_provider.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/browser/storage_partition.h"

namespace {
constexpr char kServiceName[] = "SharingService";

// Removes old encryption info with empty authorized_entity to avoid DCHECK.
// See http://crbug/987591
void CleanEncryptionInfoWithoutAuthorizedEntity(gcm::GCMDriver* gcm_driver) {
  gcm::GCMEncryptionProvider* encryption_provider =
      gcm_driver->GetEncryptionProviderInternal();
  if (!encryption_provider)
    return;

  encryption_provider->RemoveEncryptionInfo(kSharingFCMAppID,
                                            /*authorized_entity=*/std::string(),
                                            /*callback=*/base::DoNothing());
}

}  // namespace

// static
SharingServiceFactory* SharingServiceFactory::GetInstance() {
  return base::Singleton<SharingServiceFactory>::get();
}

// static
SharingService* SharingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<SharingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

SharingServiceFactory::SharingServiceFactory()
    // Sharing features are disabled in incognito.
    : ProfileKeyedServiceFactory(
          kServiceName,
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
  DependsOn(instance_id::InstanceIDProfileServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(SharingMessageBridgeFactory::GetInstance());
}

SharingServiceFactory::~SharingServiceFactory() = default;

KeyedService* SharingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);

  if (!sync_service)
    return nullptr;

#if BUILDFLAG(ENABLE_CLICK_TO_CALL)
  PrecompilePhoneNumberRegexesAsync();
#endif  // BUILDFLAG(ENABLE_CLICK_TO_CALL)

  syncer::DeviceInfoSyncService* device_info_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);
  auto sync_prefs = std::make_unique<SharingSyncPreference>(
      profile->GetPrefs(), device_info_sync_service);

  auto vapid_key_manager =
      std::make_unique<VapidKeyManager>(sync_prefs.get(), sync_service);

  instance_id::InstanceIDProfileService* instance_id_service =
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile);
  auto sharing_device_registration =
      std::make_unique<SharingDeviceRegistration>(
          profile->GetPrefs(), sync_prefs.get(), vapid_key_manager.get(),
          instance_id_service->driver(), sync_service);

  auto web_push_sender = std::make_unique<WebPushSender>(
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
  SharingMessageBridge* message_bridge =
      SharingMessageBridgeFactory::GetForBrowserContext(profile);
  gcm::GCMDriver* gcm_driver =
      gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver();
  CleanEncryptionInfoWithoutAuthorizedEntity(gcm_driver);
  syncer::LocalDeviceInfoProvider* local_device_info_provider =
      device_info_sync_service->GetLocalDeviceInfoProvider();
  auto fcm_sender = std::make_unique<SharingFCMSender>(
      std::move(web_push_sender), message_bridge, sync_prefs.get(),
      vapid_key_manager.get(), gcm_driver, local_device_info_provider,
      sync_service);

  auto sharing_message_sender =
      std::make_unique<SharingMessageSender>(local_device_info_provider);
  SharingFCMSender* fcm_sender_ptr = fcm_sender.get();
  sharing_message_sender->RegisterSendDelegate(
      SharingMessageSender::DelegateType::kFCM, std::move(fcm_sender));

  syncer::DeviceInfoTracker* device_info_tracker =
      device_info_sync_service->GetDeviceInfoTracker();
  auto device_source = std::make_unique<SharingDeviceSourceSync>(
      sync_service, local_device_info_provider, device_info_tracker);

  content::SmsFetcher* sms_fetcher = content::SmsFetcher::Get(context);
  auto handler_registry = std::make_unique<SharingHandlerRegistryImpl>(
      profile, sharing_device_registration.get(), sharing_message_sender.get(),
      device_source.get(), sms_fetcher);

  auto fcm_handler = std::make_unique<SharingFCMHandler>(
      gcm_driver, device_info_tracker, fcm_sender_ptr, handler_registry.get());

  return new SharingService(
      std::move(sync_prefs), std::move(vapid_key_manager),
      std::move(sharing_device_registration), std::move(sharing_message_sender),
      std::move(device_source), std::move(handler_registry),
      std::move(fcm_handler), sync_service);
}

bool SharingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
