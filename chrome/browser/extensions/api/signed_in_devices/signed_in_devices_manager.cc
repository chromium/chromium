// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/signed_in_devices/signed_in_devices_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/signed_in_devices/signed_in_devices_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/common/extensions/api/signed_in_devices.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension.h"

using syncer::DeviceInfo;
namespace extensions {

namespace {
void FillDeviceInfo(const DeviceInfo& device_info,
                    api::signed_in_devices::DeviceInfo* api_device_info) {
  api_device_info->id = device_info.public_id();
  api_device_info->name = device_info.client_name();
  api_device_info->os = api::signed_in_devices::ParseOS(
      device_info.GetOSString());
  api_device_info->type = api::signed_in_devices::ParseDeviceType(
      device_info.GetDeviceTypeString());
  api_device_info->chrome_version = device_info.chrome_version();
}
}  // namespace

SignedInDevicesChangeObserver::SignedInDevicesChangeObserver(
    const std::string& extension_id,
    Profile* profile) : extension_id_(extension_id),
                        profile_(profile) {
  syncer::DeviceInfoSyncService* service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile_);
  if (service) {
    DCHECK(service->GetDeviceInfoTracker());
    service->GetDeviceInfoTracker()->AddObserver(this);
  }
}

SignedInDevicesChangeObserver::~SignedInDevicesChangeObserver() {
  syncer::DeviceInfoSyncService* service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile_);
  if (service) {
    DCHECK(service->GetDeviceInfoTracker());
    service->GetDeviceInfoTracker()->RemoveObserver(this);
  }
}

void SignedInDevicesChangeObserver::OnDeviceInfoChange() {
  // There is a change in the list of devices. Get all devices and send them to
  // the listener.
  std::vector<std::unique_ptr<DeviceInfo>> devices =
      GetAllSignedInDevices(extension_id_, profile_);

  std::vector<api::signed_in_devices::DeviceInfo> args;
  for (const std::unique_ptr<DeviceInfo>& info : devices) {
    api::signed_in_devices::DeviceInfo api_device;
    FillDeviceInfo(*info, &api_device);
    args.push_back(std::move(api_device));
  }

  std::unique_ptr<base::ListValue> result =
      api::signed_in_devices::OnDeviceInfoChange::Create(args);
  auto event = std::make_unique<Event>(
      events::SIGNED_IN_DEVICES_ON_DEVICE_INFO_CHANGE,
      api::signed_in_devices::OnDeviceInfoChange::kEventName, std::move(result),
      profile_);

  EventRouter::Get(profile_)
      ->DispatchEventToExtension(extension_id_, std::move(event));
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<SignedInDevicesManager>>::DestructorAtExit
    g_signed_in_devices_manager_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<SignedInDevicesManager>*
SignedInDevicesManager::GetFactoryInstance() {
  return g_signed_in_devices_manager_factory.Pointer();
}

SignedInDevicesManager::SignedInDevicesManager() = default;
SignedInDevicesManager::SignedInDevicesManager(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  EventRouter* router = EventRouter::Get(profile_);
  if (router) {
    router->RegisterObserver(
        this, api::signed_in_devices::OnDeviceInfoChange::kEventName);
  }

  // Register for unload event so we could clear all our listeners when
  // extensions have unloaded.
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
}

SignedInDevicesManager::~SignedInDevicesManager() = default;

void SignedInDevicesManager::Shutdown() {
  if (profile_) {
    EventRouter* router = EventRouter::Get(profile_);
    if (router)
      router->UnregisterObserver(this);
  }
}

void SignedInDevicesManager::OnListenerAdded(
    const EventListenerInfo& details) {
  for (const std::unique_ptr<SignedInDevicesChangeObserver>& observer :
       change_observers_) {
    if (observer->extension_id() == details.extension_id) {
      DCHECK(false) <<"OnListenerAded fired twice for same extension";
      return;
    }
  }

  change_observers_.push_back(std::make_unique<SignedInDevicesChangeObserver>(
      details.extension_id, profile_));
}

void SignedInDevicesManager::OnListenerRemoved(
    const EventListenerInfo& details) {
  RemoveChangeObserverForExtension(details.extension_id);
}

void SignedInDevicesManager::RemoveChangeObserverForExtension(
    const std::string& extension_id) {
  for (auto it = change_observers_.begin(); it != change_observers_.end();
       ++it) {
    if ((*it)->extension_id() == extension_id) {
      change_observers_.erase(it);
      return;
    }
  }
}

void SignedInDevicesManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  RemoveChangeObserverForExtension(extension->id());
}

}  // namespace extensions
