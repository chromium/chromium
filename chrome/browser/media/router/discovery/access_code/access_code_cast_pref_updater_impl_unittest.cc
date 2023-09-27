// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater_impl.h"

#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class AccessCodeCastPrefUpdaterImplTest : public testing::Test {
 public:
  AccessCodeCastPrefUpdaterImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    RegisterAccessCodeProfilePrefs(prefs_.registry());
  }

  void SetUp() override {
    pref_updater_ = std::make_unique<AccessCodeCastPrefUpdaterImpl>(prefs());
  }

  void UpdateDevicesDict(const MediaSinkInternal& sink) {
    base::RunLoop loop;
    pref_updater()->UpdateDevicesDict(sink, loop.QuitClosure());
    loop.Run();
  }

  void UpdateDeviceAddedTimeDict(const MediaSink::Id& sink_id) {
    base::RunLoop loop;
    pref_updater()->UpdateDeviceAddedTimeDict(sink_id, loop.QuitClosure());
    loop.Run();
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

  AccessCodeCastPrefUpdaterImpl* pref_updater() { return pref_updater_.get(); }

  content::BrowserTaskEnvironment& task_env() { return task_environment_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<AccessCodeCastPrefUpdaterImpl> pref_updater_;
};

TEST_F(AccessCodeCastPrefUpdaterImplTest, TestUpdateDevicesDictRecorded) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  UpdateDevicesDict(cast_sink);
  auto& dict = prefs()->GetDict(prefs::kAccessCodeCastDevices);
  auto* sink_id_dict = dict.Find(cast_sink.id());
  EXPECT_EQ(*sink_id_dict, CreateValueDictFromMediaSinkInternal(cast_sink));
}

TEST_F(AccessCodeCastPrefUpdaterImplTest, TestUpdateDevicesDictOverwrite) {
  MediaSinkInternal cast_sink = CreateCastSink(1);

  // Store cast_sink.
  UpdateDevicesDict(cast_sink);

  // Make new cast_sink with same id, but change display name.
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  auto sink1 = cast_sink1.sink();
  sink1.set_name("new_name");
  cast_sink1.set_sink(sink1);

  // Store new cast_sink1 with same id as cast_sink, it should overwrite the
  // existing pref.
  UpdateDevicesDict(cast_sink1);

  auto& dict = prefs()->GetDict(prefs::kAccessCodeCastDevices);
  auto* sink_id_dict = dict.Find(cast_sink.id());
  EXPECT_NE(*sink_id_dict, CreateValueDictFromMediaSinkInternal(cast_sink));
  EXPECT_EQ(*sink_id_dict, CreateValueDictFromMediaSinkInternal(cast_sink1));
}

TEST_F(AccessCodeCastPrefUpdaterImplTest, TestUpdateDeviceAddedTimeDict) {
  MediaSinkInternal cast_sink = CreateCastSink(1);

  UpdateDeviceAddedTimeDict(cast_sink.id());
  auto& dict = prefs()->GetDict(prefs::kAccessCodeCastDeviceAdditionTime);
  auto* time_of_addition = dict.Find(cast_sink.id());
  EXPECT_TRUE(time_of_addition);
}

TEST_F(AccessCodeCastPrefUpdaterImplTest,
       TestUpdateDeviceAddedTimeDictOverwrite) {
  MediaSinkInternal cast_sink = CreateCastSink(1);

  UpdateDeviceAddedTimeDict(cast_sink.id());
  auto& dict = prefs()->GetDict(prefs::kAccessCodeCastDeviceAdditionTime);
  auto initial_time_of_addition =
      base::ValueToTime(dict.Find(cast_sink.id())).value();

  task_env().AdvanceClock(base::Seconds(10));
  UpdateDeviceAddedTimeDict(cast_sink.id());
  auto final_time_of_addition =
      base::ValueToTime(dict.Find(cast_sink.id())).value();

  // Expect the two times of addition to be different, and the second time to be
  // greater.
  EXPECT_GE(final_time_of_addition, initial_time_of_addition);
}

TEST_F(AccessCodeCastPrefUpdaterImplTest,
       TestGetMediaSinkInternalValueBySinkId) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  UpdateDevicesDict(cast_sink);
  base::test::TestFuture<base::Value::Dict> cast_sink_from_dict;
  pref_updater()->GetMediaSinkInternalValueBySinkId(
      cast_sink.id(), cast_sink_from_dict.GetCallback());
  EXPECT_FALSE(cast_sink_from_dict.Get().empty());
  EXPECT_EQ(CreateValueDictFromMediaSinkInternal(cast_sink),
            cast_sink_from_dict.Get());

  base::test::TestFuture<base::Value::Dict> cast_sink_from_dict2;
  pref_updater()->GetMediaSinkInternalValueBySinkId(
      cast_sink2.id(), cast_sink_from_dict2.GetCallback());
  EXPECT_TRUE(cast_sink_from_dict2.Get().empty());
}

TEST_F(AccessCodeCastPrefUpdaterImplTest, TestRemoveSinkIdFromDevicesDict) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  UpdateDevicesDict(cast_sink);

  pref_updater()->RemoveSinkIdFromDevicesDict(cast_sink.id(),
                                              base::DoNothing());
  auto& dict = prefs()->GetDict(prefs::kAccessCodeCastDevices);
  EXPECT_FALSE(dict.Find(cast_sink.id()));
  pref_updater()->RemoveSinkIdFromDevicesDict(cast_sink2.id(),
                                              base::DoNothing());
}

TEST_F(AccessCodeCastPrefUpdaterImplTest,
       TestRemoveSinkIdFromDeviceAddedTimeDict) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  UpdateDeviceAddedTimeDict(cast_sink.id());

  pref_updater()->RemoveSinkIdFromDeviceAddedTimeDict(cast_sink.id(),
                                                      base::DoNothing());
  auto& dict = prefs()->GetDict(prefs::kAccessCodeCastDeviceAdditionTime);
  EXPECT_FALSE(dict.Find(cast_sink.id()));

  pref_updater()->RemoveSinkIdFromDeviceAddedTimeDict(cast_sink2.id(),
                                                      base::DoNothing());
}

TEST_F(AccessCodeCastPrefUpdaterImplTest, TestGetDeviceAddedTime) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  UpdateDeviceAddedTimeDict(cast_sink.id());
  base::test::TestFuture<base::Value::Dict> device_added_time_dict;
  pref_updater()->GetDeviceAddedTimeDict(device_added_time_dict.GetCallback());
  EXPECT_TRUE(device_added_time_dict.Get().contains(cast_sink.id()));
  EXPECT_FALSE(device_added_time_dict.Get().contains(cast_sink2.id()));
}

TEST_F(AccessCodeCastPrefUpdaterImplTest, TestClearDevicesDict) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  UpdateDevicesDict(cast_sink);
  UpdateDevicesDict(cast_sink2);
  {
    base::test::TestFuture<base::Value::Dict> devices_dict;
    pref_updater()->GetDevicesDict(devices_dict.GetCallback());
    EXPECT_FALSE(devices_dict.Get().empty());
  }

  pref_updater()->ClearDevicesDict(base::DoNothing());
  {
    base::test::TestFuture<base::Value::Dict> devices_dict;
    pref_updater()->GetDevicesDict(devices_dict.GetCallback());
    EXPECT_TRUE(devices_dict.Get().empty());
  }
}

TEST_F(AccessCodeCastPrefUpdaterImplTest, TestClearDeviceAddedTimeDict) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  UpdateDeviceAddedTimeDict(cast_sink.id());
  UpdateDeviceAddedTimeDict(cast_sink2.id());
  {
    base::test::TestFuture<base::Value::Dict> device_added_time_dict;
    pref_updater()->GetDeviceAddedTimeDict(
        device_added_time_dict.GetCallback());
    EXPECT_FALSE(device_added_time_dict.Get().empty());
  }

  pref_updater()->ClearDeviceAddedTimeDict(base::DoNothing());
  {
    base::test::TestFuture<base::Value::Dict> device_added_time_dict;
    pref_updater()->GetDeviceAddedTimeDict(
        device_added_time_dict.GetCallback());
    EXPECT_TRUE(device_added_time_dict.Get().empty());
  }
}

TEST_F(AccessCodeCastPrefUpdaterImplTest, TestUpdateDevicesDictIdenticalIPs) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);
  MediaSinkInternal cast_sink3 = CreateCastSink(3);

  // Set the ip_endpoint of cast_sink2 to the ip_endpoint of cast_sink.
  cast_sink2.set_cast_data(cast_sink.cast_data());

  UpdateDevicesDict(cast_sink);

  // This add will overwrite the original storage of the cast_sink, since
  // cast_sink2 has the same ip_endpoint.
  UpdateDevicesDict(cast_sink2);
  UpdateDevicesDict(cast_sink3);

  // There should only be two devices stored since two ip_endpoints were
  // identical.
  base::test::TestFuture<base::Value::Dict> devices_dict;
  pref_updater()->GetDevicesDict(devices_dict.GetCallback());
  EXPECT_EQ(devices_dict.Get().size(), 2u);
  EXPECT_TRUE(devices_dict.Get().contains(cast_sink2.id()));
  EXPECT_TRUE(devices_dict.Get().contains(cast_sink3.id()));
}

TEST_F(AccessCodeCastPrefUpdaterImplTest, TestUpdateDevicesDictDifferentIPs) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);
  MediaSinkInternal cast_sink3 = CreateCastSink(3);

  UpdateDevicesDict(cast_sink);
  UpdateDevicesDict(cast_sink2);
  UpdateDevicesDict(cast_sink3);

  // There should only be two devices stored since two ip_endpoints were
  // identical.
  base::test::TestFuture<base::Value::Dict> devices_dict;
  pref_updater()->GetDevicesDict(devices_dict.GetCallback());
  EXPECT_EQ(devices_dict.Get().size(), 3u);
  EXPECT_TRUE(devices_dict.Get().contains(cast_sink.id()));
  EXPECT_TRUE(devices_dict.Get().contains(cast_sink2.id()));
  EXPECT_TRUE(devices_dict.Get().contains(cast_sink3.id()));
}

}  // namespace media_router
