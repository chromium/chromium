// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_provisioning_service/app_provisioning_data_manager.h"

#include <memory>

#include "base/logging.h"
#include "chrome/browser/apps/app_provisioning_service/proto/app_data.pb.h"

namespace apps {

// static
AppProvisioningDataManager* AppProvisioningDataManager::Get() {
  static base::NoDestructor<AppProvisioningDataManager> instance;
  return instance.get();
}

AppProvisioningDataManager::AppProvisioningDataManager() = default;

AppProvisioningDataManager::~AppProvisioningDataManager() = default;

void AppProvisioningDataManager::PopulateFromDynamicUpdate(
    const std::string& binary_pb,
    const base::FilePath& install_dir) {
  // Parse the proto and do some validation on it.
  if (binary_pb.empty()) {
    LOG(ERROR) << "Binary is empty";
    return;
  }

  std::unique_ptr<proto::AppWithLocaleList> app_with_locale_list =
      std::make_unique<proto::AppWithLocaleList>();
  if (!app_with_locale_list->ParseFromString(binary_pb)) {
    LOG(ERROR) << "Failed to parse protobuf";
    return;
  }

  // TODO(melzhang) : Add check that version of |app_with_locale_list| is newer.
  app_with_locale_list_ = std::move(app_with_locale_list);
  data_dir_ = install_dir;
  OnAppDataUpdated();
}

const base::FilePath& AppProvisioningDataManager::GetDataFilePath() {
  return data_dir_;
}

void AppProvisioningDataManager::OnAppDataUpdated() {
  if (!app_with_locale_list_) {
    return;
  }
  for (auto& observer : observers_) {
    NotifyObserver(observer);
  }
}

void AppProvisioningDataManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  if (app_with_locale_list_) {
    NotifyObserver(*observer);
  }
}

void AppProvisioningDataManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppProvisioningDataManager::NotifyObserver(Observer& observer) {
  observer.OnAppWithLocaleListUpdated(*app_with_locale_list_.get());
}

}  // namespace apps
