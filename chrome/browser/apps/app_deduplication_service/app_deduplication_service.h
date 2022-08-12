// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVICE_H_

#include <map>
#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_deduplication_service/duplicate_group.h"
#include "chrome/browser/apps/app_deduplication_service/entry_types.h"
#include "chrome/browser/apps/app_provisioning_service/app_provisioning_data_manager.h"
#include "chrome/browser/apps/app_provisioning_service/proto/app_data.pb.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace apps::deduplication {

class AppDeduplicationService : public KeyedService,
                                public AppProvisioningDataManager::Observer {
 public:
  explicit AppDeduplicationService(Profile* profile);
  ~AppDeduplicationService() override;
  AppDeduplicationService(const AppDeduplicationService&) = delete;
  AppDeduplicationService& operator=(const AppDeduplicationService&) = delete;

  std::vector<Entry> GetDuplicates(const EntryId& entry_id);
  bool AreDuplicates(const EntryId& entry_id_1, const EntryId& entry_id_2);

 private:
  friend class AppDeduplicationServiceTest;
  FRIEND_TEST_ALL_PREFIXES(AppDeduplicationServiceTest,
                           OnDuplicatedAppsMapUpdated);
  FRIEND_TEST_ALL_PREFIXES(AppDeduplicationServiceTest, ExactDuplicate);

  // AppProvisioningDataManager::Observer:
  void OnDuplicatedAppsMapUpdated(
      const proto::DuplicatedAppsMap& duplicated_apps_map) override;

  std::map<std::string, DuplicateGroup> duplication_map_;
  std::map<EntryId, std::string> entry_to_group_map_;

  base::ScopedObservation<AppProvisioningDataManager,
                          AppProvisioningDataManager::Observer>
      app_provisioning_data_observeration_{this};
};

}  // namespace apps::deduplication

#endif  // CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVICE_H_
