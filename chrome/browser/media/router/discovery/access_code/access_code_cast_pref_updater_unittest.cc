// Copyright 2022 The Chromium Authors
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
  auto& dict = prefs()->GetDict(prefs::kAccessCodeCastDevices);
  auto* sink_id_dict = dict.Find(cast_sink.id());
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

  auto& dict = prefs()->GetDict(prefs::kAccessCodeCastDevices);
  auto* sink_id_dict = dict.Find(cast_sink.id());
  EXPECT_NE(*sink_id_dict, CreateValueDictFromMediaSinkInternal(cast_sink));
  EXPECT_EQ(*sink_id_dict, CreateValueDictFromMediaSinkInternal(cast_sink1));
}

TEST_F(AccessCodeCastPrefUpdaterTest, TestUpdateDeviceAddedTimeDict) {
  MediaSinkInternal cast_sink = CreateCastSink(1);

  pref_updater()->UpdateDeviceAddedTimeDict(cast_sink.id());
  auto& dict = prefs()->GetDict(prefs::kAccessCodeCastDeviceAdditionTime);
  auto* time_of_addition = dict.Find(cast_sink.id());
  EXPECT_TRUE(time_of_addition);
}

TEST_F(AccessCodeCastPrefUpdaterTest, TestUpdateDeviceAddedTimeDictOverwrite) {
  MediaSinkInternal cast_sink = CreateCastSink(1);

  pref_updater()->UpdateDeviceAddedTimeDict(cast_sink.id());
  auto& dict = prefs()->GetDict(prefs::kAccessCodeCastDeviceAdditionTime);
  auto initial_time_of_addition =
      base::ValueToTime(dict.Find(cast_sink.id())).value();

  task_env().AdvanceClock(base::Seconds(10));
  pref_updater()->UpdateDeviceAddedTimeDict(cast_sink.id());
  auto final_time_of_addition =
      base::ValueToTime(dict.Find(cast_sink.id())).value();

  // Expect the two times of addition to be different, and the second time to be
  // greater.
  EXPECT_GE(final_time_of_addition, initial_time_of_addition);
}

TEST_F(AccessCodeCastPrefUpdaterTest, TestGetMediaSinkInternalValueBySinkId) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  pref_updater()->UpdateDevicesDict(cast_sink);

  EXPECT_TRUE(
      pref_updater()->GetMediaSinkInternalValueBySinkId(cast_sink.id()));
  EXPECT_FALSE(
      pref_updater()->GetMediaSinkInternalValueBySinkId(cast_sink2.id()));
}

TEST_F(AccessCodeCastPrefUpdaterTest, TestRemoveSinkIdFromDevicesDict) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  pref_updater()->UpdateDevicesDict(cast_sink);

  pref_updater()->RemoveSinkIdFromDevicesDict(cast_sink.id());
  auto& dict = prefs()->GetDict(prefs::kAccessCodeCastDevices);
  EXPECT_FALSE(dict.Find(cast_sink.id()));
  pref_updater()->RemoveSinkIdFromDevicesDict(cast_sink2.id());
}

TEST_F(AccessCodeCastPrefUpdaterTest, TestRemoveSinkIdFromDeviceAddedTimeDict) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  pref_updater()->UpdateDeviceAddedTimeDict(cast_sink.id());

  pref_updater()->RemoveSinkIdFromDeviceAddedTimeDict(cast_sink.id());
  auto& dict = prefs()->GetDict(prefs::kAccessCodeCastDeviceAdditionTime);
  EXPECT_FALSE(dict.Find(cast_sink.id()));

  pref_updater()->RemoveSinkIdFromDeviceAddedTimeDict(cast_sink2.id());
}

TEST_F(AccessCodeCastPrefUpdaterTest, TestGetDeviceAddedTime) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  pref_updater()->UpdateDeviceAddedTimeDict(cast_sink.id());

  EXPECT_TRUE(pref_updater()->GetDeviceAddedTime(cast_sink.id()));
  EXPECT_FALSE(pref_updater()->GetDeviceAddedTime(cast_sink2.id()));
}

TEST_F(AccessCodeCastPrefUpdaterTest, TestGetSinkIdsFromDevicesDict) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  pref_updater()->UpdateDevicesDict(cast_sink);
  pref_updater()->UpdateDevicesDict(cast_sink2);

  auto expected_sink_ids = base::Value::List();
  expected_sink_ids.Append(cast_sink.id());
  expected_sink_ids.Append(cast_sink2.id());

  EXPECT_EQ(pref_updater()->GetSinkIdsFromDevicesDict(), expected_sink_ids);
}

TEST_F(AccessCodeCastPrefUpdaterTest, TestClearDevicesDict) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  pref_updater()->UpdateDevicesDict(cast_sink);
  pref_updater()->UpdateDevicesDict(cast_sink2);

  EXPECT_FALSE(pref_updater()->GetDevicesDict().empty());

  pref_updater()->ClearDevicesDict();

  EXPECT_TRUE(pref_updater()->GetDevicesDict().empty());
}

TEST_F(AccessCodeCastPrefUpdaterTest, TestClearDeviceAddedTimeDict) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  pref_updater()->UpdateDeviceAddedTimeDict(cast_sink.id());
  pref_updater()->UpdateDeviceAddedTimeDict(cast_sink2.id());

  EXPECT_FALSE(pref_updater()->GetDeviceAddedTimeDict().empty());

  pref_updater()->ClearDeviceAddedTimeDict();

  EXPECT_TRUE(pref_updater()->GetDeviceAddedTimeDict().empty());
}

}  // namespace media_router
