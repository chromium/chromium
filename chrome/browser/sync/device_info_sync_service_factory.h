// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_

#include <vector>

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace syncer {
class DeviceInfoSyncService;
class DeviceInfoTracker;
}  // namespace syncer

class DeviceInfoSyncServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static syncer::DeviceInfoSyncService* GetForProfile(Profile* profile);
  static DeviceInfoSyncServiceFactory* GetInstance();

  DeviceInfoSyncServiceFactory(const DeviceInfoSyncServiceFactory&) = delete;
  DeviceInfoSyncServiceFactory& operator=(const DeviceInfoSyncServiceFactory&) =
      delete;

  // Iterates over all of the profiles that have been loaded so far, and
  // extracts their tracker if present. If some profiles don't have trackers, no
  // indication is given in the passed vector.
  static void GetAllDeviceInfoTrackers(
      std::vector<const syncer::DeviceInfoTracker*>* trackers);

 private:
  friend base::NoDestructor<DeviceInfoSyncServiceFactory>;

  DeviceInfoSyncServiceFactory();
  ~DeviceInfoSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SYNC_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_
