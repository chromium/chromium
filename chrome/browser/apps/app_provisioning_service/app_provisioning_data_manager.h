// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_PROVISIONING_SERVICE_APP_PROVISIONING_DATA_MANAGER_H_
#define CHROME_BROWSER_APPS_APP_PROVISIONING_SERVICE_APP_PROVISIONING_DATA_MANAGER_H_

#include <string>

#include "base/no_destructor.h"

namespace apps {

// The AppProvisioningDataManager parses the updates received from the Component
// Updater and forwards the data in the desired format to the relevant service.
// E.g. Component Updater sends through new discovery app data, after parsing
// and formatting the proto, this class would then send the update to the App
// Discovery Service.
class AppProvisioningDataManager {
 public:
  static AppProvisioningDataManager* Get();

  AppProvisioningDataManager(const AppProvisioningDataManager&) = delete;
  AppProvisioningDataManager& operator=(const AppProvisioningDataManager&) =
      delete;
  // Note that AppProvisioningDataManager is a NoDestructor and thus never
  // destroyed.
  virtual ~AppProvisioningDataManager();

  static AppProvisioningDataManager* GetInstance();  // Singleton

  // Update the internal list from a binary proto fetched from the network.
  // Same integrity checks apply. This can be called multiple times with new
  // protos.
  void PopulateFromDynamicUpdate(const std::string& binary_pb);

 protected:
  // Creator must call one of Populate* before calling other methods.
  AppProvisioningDataManager();

 private:
  friend class base::NoDestructor<AppProvisioningDataManager>;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_PROVISIONING_SERVICE_APP_PROVISIONING_DATA_MANAGER_H_
