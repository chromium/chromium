// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater.h"

#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {
namespace {
const std::string& network1 = "foo_network";
const std::string& network2 = "bar_network";
}  // namespace

class AccessCodeCastPrefUpdaterTest : public testing::Test {
 public:
  AccessCodeCastPrefUpdaterTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    RegisterAccessCodeProfilePrefs(prefs_.registry());
  }

  void SetUp() override {
    pref_updater_ = std::make_unique<AccessCodeCastPrefUpdater>(prefs());
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

  AccessCodeCastPrefUpdater* pref_updater() { return pref_updater_.get(); }

  content::BrowserTaskEnvironment& task_env() { return task_environment_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<AccessCodeCastPrefUpdater> pref_updater_;
};

TEST_F(AccessCodeCastPrefUpdaterTest, TestUpdateDevicesDictRecorded) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  pref_updater()->UpdateDevicesDict(cast_sink);
  auto* dict = prefs()->GetDictionary(prefs::kAccessCodeCastDevices);
  auto* sink_id_dict = dict->FindKey(cast_sink.id());
  EXPECT_EQ(*sink_id_dict, CreateValueDictFromMediaSinkInternal(cast_sink));
}

TEST_F(AccessCodeCastPrefUpdaterTest, TestUpdateDevicesDictOverwrite) {
  MediaSinkInternal cast_sink = CreateCastSink(1);

  // Store cast_sink.
  pref_updater()->UpdateDevicesDict(cast_sink);

  // Make new cast_sink with same id, but change display name.
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  auto sink1 = cast_sink1.sink();
  sink1.set_name("new_name");
  cast_sink1.set_sink(sink1);

  // Store new cast_sink1 with same id as cast_sink, it should overwrite the
  // existing pref.
  pref_updater()->UpdateDevicesDict(cast_sink1);

  auto* dict = prefs()->GetDictionary(prefs::kAccessCodeCastDevices);
  auto* sink_id_dict = dict->FindKey(cast_sink.id());
  EXPECT_NE(*sink_id_dict, CreateValueDictFromMediaSinkInternal(cast_sink));
  EXPECT_EQ(*sink_id_dict, CreateValueDictFromMediaSinkInternal(cast_sink1));
}

TEST_F(AccessCodeCastPrefUpdaterTest, UpdateDiscoveredNetworksDictRecorded) {
  MediaSinkInternal cast_sink = CreateCastSink(1);

  pref_updater()->UpdateDiscoveredNetworksDict(cast_sink.id(), network1);

  auto expected_network_list =
      std::make_unique<base::Value>(base::Value::Type::LIST);
  expected_network_list->Append(cast_sink.id());

  auto* dict = prefs()->GetDictionary(prefs::kAccessCodeCastDiscoveredNetworks);
  auto* network_list = dict->FindKey(network1);

  EXPECT_EQ(*network_list, *expected_network_list.get());
}

TEST_F(AccessCodeCastPrefUpdaterTest,
       UpdateDiscoveredNetworksDictMultipleSinks) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  pref_updater()->UpdateDiscoveredNetworksDict(cast_sink.id(), network1);
  pref_updater()->UpdateDiscoveredNetworksDict(cast_sink2.id(), network1);

  auto expected_network_list =
      std::make_unique<base::Value>(base::Value::Type::LIST);
  expected_network_list->Append(cast_sink.id());
  expected_network_list->Append(cast_sink2.id());

  auto* dict = prefs()->GetDictionary(prefs::kAccessCodeCastDiscoveredNetworks);
  auto* network_list = dict->FindKey(network1);

  EXPECT_EQ(*network_list, *expected_network_list.get());
}

TEST_F(AccessCodeCastPrefUpdaterTest,
       UpdateDiscoveredNetworksDictMultipleNetworks) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);
  MediaSinkInternal cast_sink3 = CreateCastSink(3);
  MediaSinkInternal cast_sink4 = CreateCastSink(4);

  pref_updater()->UpdateDiscoveredNetworksDict(cast_sink.id(), network1);
  pref_updater()->UpdateDiscoveredNetworksDict(cast_sink2.id(), network1);
  pref_updater()->UpdateDiscoveredNetworksDict(cast_sink3.id(), network2);
  pref_updater()->UpdateDiscoveredNetworksDict(cast_sink4.id(), network2);

  auto expected_network1_list =
      std::make_unique<base::Value>(base::Value::Type::LIST);
  expected_network1_list->Append(cast_sink.id());
  expected_network1_list->Append(cast_sink2.id());

  auto expected_network2_list =
      std::make_unique<base::Value>(base::Value::Type::LIST);
  expected_network2_list->Append(cast_sink3.id());
  expected_network2_list->Append(cast_sink4.id());

  auto* dict = prefs()->GetDictionary(prefs::kAccessCodeCastDiscoveredNetworks);

  auto* network_list1 = dict->FindKey(network1);
  EXPECT_EQ(*network_list1, *expected_network1_list.get());

  auto* network_list2 = dict->FindKey(network2);
  EXPECT_EQ(*network_list2, *expected_network2_list.get());
}

TEST_F(AccessCodeCastPrefUpdaterTest, EnsureNoDuplicatesInSameNetworkList) {
  MediaSinkInternal cast_sink = CreateCastSink(1);

  pref_updater()->UpdateDiscoveredNetworksDict(cast_sink.id(), network1);
  pref_updater()->UpdateDiscoveredNetworksDict(cast_sink.id(), network1);

  auto expected_network_list =
      std::make_unique<base::Value>(base::Value::Type::LIST);
  expected_network_list->Append(cast_sink.id());

  auto* dict = prefs()->GetDictionary(prefs::kAccessCodeCastDiscoveredNetworks);
  auto* network_list = dict->FindKey(network1);

  EXPECT_EQ(*network_list, *expected_network_list.get());
}

TEST_F(AccessCodeCastPrefUpdaterTest, TestUpdateDeviceAdditionTimeDict) {
  MediaSinkInternal cast_sink = CreateCastSink(1);

  pref_updater()->UpdateDeviceAdditionTimeDict(cast_sink.id());
  auto* dict = prefs()->GetDictionary(prefs::kAccessCodeCastDeviceAdditionTime);
  auto* time_of_addition = dict->FindKey(cast_sink.id());
  EXPECT_TRUE(time_of_addition);
}

TEST_F(AccessCodeCastPrefUpdaterTest,
       TestUpdateDeviceAdditionTimeDictOverwrite) {
  MediaSinkInternal cast_sink = CreateCastSink(1);

  pref_updater()->UpdateDeviceAdditionTimeDict(cast_sink.id());
  auto* dict = prefs()->GetDictionary(prefs::kAccessCodeCastDeviceAdditionTime);
  auto initial_time_of_addition =
      base::ValueToTime(dict->FindKey(cast_sink.id())).value();

  task_env().AdvanceClock(base::Seconds(10));
  pref_updater()->UpdateDeviceAdditionTimeDict(cast_sink.id());
  auto final_time_of_addition =
      base::ValueToTime(dict->FindKey(cast_sink.id())).value();

  // Expect the two times of addition to be different, and the second time to be
  // greater.
  EXPECT_GE(final_time_of_addition, initial_time_of_addition);
}

}  // namespace media_router
