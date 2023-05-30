// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_PROVISIONING_SERVICE_APP_PROVISIONING_DATA_MANAGER_H_
#define CHROME_BROWSER_APPS_APP_PROVISIONING_SERVICE_APP_PROVISIONING_DATA_MANAGER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "chrome/browser/apps/app_provisioning_service/proto/app_data.pb.h"

namespace apps {

struct ComponentFileContents {
  std::string app_with_locale_pb;
};

// The AppProvisioningDataManager parses the updates received from the Component
// Updater and forwards the data in the desired format to the relevant service.
// E.g. Component Updater sends through new discovery app data, after parsing
// and formatting the proto, this class would then send the update to the App
// Discovery Service.
class AppProvisioningDataManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAppWithLocaleListUpdated(
        const proto::AppWithLocaleList& app_with_locale_list) {}
  };

  static AppProvisioningDataManager* Get();

  AppProvisioningDataManager(const AppProvisioningDataManager&) = delete;
  AppProvisioningDataManager& operator=(const AppProvisioningDataManager&) =
      delete;
  // Note that AppProvisioningDataManager is a NoDestructor and thus never
  // destroyed.
  virtual ~AppProvisioningDataManager();

  static AppProvisioningDataManager* GetInstance();  // Singleton

  // Update the internal list from the binary proto files fetched from the
  // network.
  // Same integrity checks apply. This can be called multiple times with new
  // protos.
  void PopulateFromDynamicUpdate(const ComponentFileContents& component_files,
                                 const base::FilePath& install_dir);

  const base::FilePath& GetDataFilePath();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  // Creator must call one of Populate* before calling other methods.
  AppProvisioningDataManager();

 private:
  friend class base::NoDestructor<AppProvisioningDataManager>;

  void OnAppDataUpdated();
  void NotifyObserver(Observer& observer);

  // The latest app data. Starts out as null.
  std::unique_ptr<proto::AppWithLocaleList> app_with_locale_list_;

  base::ObserverList<Observer> observers_;

  // The path to the directory that contains all app data, including icons.
  base::FilePath data_dir_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_PROVISIONING_SERVICE_APP_PROVISIONING_DATA_MANAGER_H_
