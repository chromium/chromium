// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_provisioning_service/app_provisioning_data_manager.h"

#include <memory>

#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_provisioning_service/proto/app_data.pb.h"
#include "chrome/common/chrome_features.h"

namespace apps {

namespace {
// TODO(b/238394602): Use fake data for now. Update to generate from real data
// when real data is ready.
std::unique_ptr<proto::DuplicatedGroupList> PopulateDuplicatedGroupList() {
  std::unique_ptr<proto::DuplicatedGroupList> duplicated_group_list =
      std::make_unique<proto::DuplicatedGroupList>();
  auto* add_group = duplicated_group_list->add_duplicate_group();
  proto::DuplicateGroup duplicate_group;
  auto* arc_app = duplicate_group.add_app();
  arc_app->set_app_id_for_platform("test_arc_app_id");
  arc_app->set_source_name("arc");

  auto* web_app = duplicate_group.add_app();
  web_app->set_app_id_for_platform("test_web_app_id");
  web_app->set_source_name("web");

  *add_group = duplicate_group;
  return duplicated_group_list;
}

std::unique_ptr<proto::AppWithLocaleList> PopulateAppWithLocaleList(
    const std::string& binary_pb) {
  // Parse the proto and do some validation on it.
  if (binary_pb.empty()) {
    LOG(ERROR) << "Binary is empty";
    return nullptr;
  }

  std::unique_ptr<proto::AppWithLocaleList> app_with_locale_list =
      std::make_unique<proto::AppWithLocaleList>();
  if (!app_with_locale_list->ParseFromString(binary_pb)) {
    LOG(ERROR) << "Failed to parse protobuf";
    return nullptr;
  }

  return app_with_locale_list;
}
}  // namespace

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
  // TODO(melzhang) : Add check that version of |app_with_locale_list| is newer.
  app_with_locale_list_ = PopulateAppWithLocaleList(binary_pb);
  if (base::FeatureList::IsEnabled(features::kAppDeduplicationService)) {
    duplicated_group_list_ = PopulateDuplicatedGroupList();
  }
  data_dir_ = install_dir;
  OnAppDataUpdated();
}

const base::FilePath& AppProvisioningDataManager::GetDataFilePath() {
  return data_dir_;
}

void AppProvisioningDataManager::OnAppDataUpdated() {
  if (!app_with_locale_list_ && !duplicated_group_list_) {
    return;
  }
  for (auto& observer : observers_) {
    NotifyObserver(observer);
  }
}

void AppProvisioningDataManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  if (app_with_locale_list_ || duplicated_group_list_) {
    NotifyObserver(*observer);
  }
}

void AppProvisioningDataManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppProvisioningDataManager::NotifyObserver(Observer& observer) {
  // TODO(b/221173736): Add version check so that only notify observer when new
  // version is available.
  if (app_with_locale_list_) {
    observer.OnAppWithLocaleListUpdated(*app_with_locale_list_.get());
  }
  // TODO(b/238394602): Add version check so that only notify observer when new
  // version is available.
  if (duplicated_group_list_) {
    observer.OnDuplicatedGroupListUpdated(*duplicated_group_list_.get());
  }
}

}  // namespace apps
