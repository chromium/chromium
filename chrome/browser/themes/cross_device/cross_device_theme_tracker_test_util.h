// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_TRACKER_TEST_UTIL_H_
#define CHROME_BROWSER_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_TRACKER_TEST_UTIL_H_

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/test_data_type_store_service.h"
#include "components/themes/cross_device/cross_device_theme_sync_bridge.h"
#include "components/themes/cross_device/cross_device_theme_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace themes {

template <typename LocalSpecifics>
class CrossDeviceThemeTrackerTestBase : public testing::Test {
 protected:
  CrossDeviceThemeTrackerTestBase()
      : tracker_(std::make_unique<CrossDeviceThemeTracker<LocalSpecifics>>()) {}

  ~CrossDeviceThemeTrackerTestBase() override = default;

  std::unique_ptr<syncer::EntityChange> CreateAddChange(
      const std::string& storage_key,
      const sync_pb::EntitySpecifics& specifics) {
    syncer::EntityData data;
    data.specifics = specifics;
    data.client_tag_hash = syncer::ClientTagHash::FromHashed(storage_key);
    return syncer::EntityChange::CreateAdd(storage_key, std::move(data));
  }

  template <typename RemoteSpecifics>
  themes::CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>*
  RegisterBridgeHelper(
      syncer::DataType type,
      base::RepeatingCallback<themes::DeviceThemeInfo<LocalSpecifics>(
          const RemoteSpecifics&)> translate_cb,
      themes::CrossDeviceThemeTracker<LocalSpecifics>* tracker,
      syncer::DataTypeStoreService* store_service) {
    syncer::RepeatingDataTypeStoreFactory store_factory =
        store_service->GetStoreFactory();
    auto processor = std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
        type, base::DoNothing());

    auto bridge = std::make_unique<
        themes::CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>>(
        type, std::move(translate_cb),
        base::BindRepeating(
            &themes::CrossDeviceThemeTracker<LocalSpecifics>::UpdateThemeInfo,
            base::Unretained(tracker)),
        base::BindRepeating(
            &themes::CrossDeviceThemeTracker<LocalSpecifics>::RemoveThemeInfo,
            base::Unretained(tracker)),
        base::BindRepeating(&themes::CrossDeviceThemeTracker<
                                LocalSpecifics>::OnBridgeSyncStarted,
                            base::Unretained(tracker), type),
        base::BindRepeating(&themes::CrossDeviceThemeTracker<
                                LocalSpecifics>::OnBridgeSyncDisabled,
                            base::Unretained(tracker), type),
        std::move(processor), store_factory);

    auto* bridge_ptr = bridge.get();
    tracker->RegisterBridge(type, std::move(bridge));
    return bridge_ptr;
  }

  base::test::TaskEnvironment task_environment_;
  syncer::TestDataTypeStoreService test_store_service_;
  std::unique_ptr<CrossDeviceThemeTracker<LocalSpecifics>> tracker_;
};

}  // namespace themes

#endif  // CHROME_BROWSER_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_TRACKER_TEST_UTIL_H_
