// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater_impl.h"

#include "base/json/values_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater_lacros.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/prefs.mojom-test-utils.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::Value::Dict GetDictFromPrefService(crosapi::mojom::PrefPath pref_path) {
  absl::optional<::base::Value> out_value;
  crosapi::mojom::PrefsAsyncWaiter async_waiter(
      chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Prefs>().get());
  async_waiter.GetPref(pref_path, &out_value);
  if (out_value && out_value->is_dict()) {
    return std::move(out_value.value()).TakeDict();
  }

  return base::Value::Dict();
}
}  // namespace

namespace media_router {

class AccessCodeCastPrefUpdaterLacrosTest : public InProcessBrowserTest {
 public:
  AccessCodeCastPrefUpdaterLacrosTest() {
    pref_updater_ = std::make_unique<AccessCodeCastPrefUpdaterLacros>();
  }

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    auto* lacros_service = chromeos::LacrosService::Get();
    ASSERT_TRUE(lacros_service);
    ASSERT_TRUE(lacros_service->IsAvailable<crosapi::mojom::Prefs>());

    crosapi::mojom::PrefsAsyncWaiter async_waiter(
        chromeos::LacrosService::Get()
            ->GetRemote<crosapi::mojom::Prefs>()
            .get());
    absl::optional<base::Value> pref_value;
    async_waiter.GetPref(crosapi::mojom::PrefPath::kAccessCodeCastDevices,
                         &pref_value);

    // If the pref cannot be fetched, the ash version may be too old.
    if (!pref_value.has_value()) {
      GTEST_SKIP() << "Skipping as the prefs are not available in the "
                      "current version of Ash";
    }
  }

  void TearDownOnMainThread() override {
    // Remove all stored devices from prefs because the same ash-chrome instance
    // is used for each test and prefs stored in ash won't be reset after each
    // test finishes.
    crosapi::mojom::PrefsAsyncWaiter async_waiter(
        chromeos::LacrosService::Get()
            ->GetRemote<crosapi::mojom::Prefs>()
            .get());
    async_waiter.SetPref(crosapi::mojom::PrefPath::kAccessCodeCastDevices,
                         base::Value(base::Value::Type::DICT));
    async_waiter.SetPref(
        crosapi::mojom::PrefPath::kAccessCodeCastDeviceAdditionTime,
        base::Value(base::Value::Type::DICT));

    ASSERT_TRUE(GetDevicesDictFromPrefService().empty());
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

  void RemoveSinkIdFromDevicesDict(const MediaSink::Id& sink_id) {
    base::RunLoop loop;
    pref_updater()->RemoveSinkIdFromDevicesDict(sink_id, loop.QuitClosure());
    loop.Run();
  }

  void RemoveSinkIdFromDeviceAddedTimeDict(const MediaSink::Id& sink_id) {
    base::RunLoop loop;
    pref_updater()->RemoveSinkIdFromDeviceAddedTimeDict(sink_id,
                                                        loop.QuitClosure());
    loop.Run();
  }

  base::Value::Dict GetDevicesDictFromPrefService() {
    return GetDictFromPrefService(
        crosapi::mojom::PrefPath::kAccessCodeCastDevices);
  }

  base::Value::Dict GetDeviceAddedTimeDictFromPrefService() {
    return GetDictFromPrefService(
        crosapi::mojom::PrefPath::kAccessCodeCastDeviceAdditionTime);
  }

  AccessCodeCastPrefUpdaterLacros* pref_updater() {
    return pref_updater_.get();
  }

 private:
  std::unique_ptr<AccessCodeCastPrefUpdaterLacros> pref_updater_;
};

IN_PROC_BROWSER_TEST_F(AccessCodeCastPrefUpdaterLacrosTest,
                       TestUpdateDevicesDictRecorded) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  UpdateDevicesDict(cast_sink);

  auto devices_dict = GetDevicesDictFromPrefService();
  const auto* sink_id_dict = devices_dict.FindDict(cast_sink.id());
  EXPECT_TRUE(sink_id_dict);
  EXPECT_EQ(*sink_id_dict, CreateValueDictFromMediaSinkInternal(cast_sink));
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastPrefUpdaterLacrosTest,
                       TestUpdateDevicesDictOverwrite) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  UpdateDevicesDict(cast_sink);

  // Make new cast_sink with same id, but change display name.
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  auto sink1 = cast_sink1.sink();
  sink1.set_name("new_name");
  cast_sink1.set_sink(sink1);

  // Store new cast_sink1 with same id as cast_sink, it should overwrite the
  // existing pref.
  UpdateDevicesDict(cast_sink1);

  auto devices_dict = GetDevicesDictFromPrefService();
  const auto* sink_id_dict = devices_dict.FindDict(cast_sink.id());
  EXPECT_NE(*sink_id_dict, CreateValueDictFromMediaSinkInternal(cast_sink));
  EXPECT_EQ(*sink_id_dict, CreateValueDictFromMediaSinkInternal(cast_sink1));
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastPrefUpdaterLacrosTest,
                       TestUpdateDeviceAddedTimeDict) {
  MediaSinkInternal cast_sink = CreateCastSink(1);

  UpdateDeviceAddedTimeDict(cast_sink.id());
  auto devices_added_time_dict = GetDeviceAddedTimeDictFromPrefService();
  EXPECT_TRUE(devices_added_time_dict.Find(cast_sink.id()));
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastPrefUpdaterLacrosTest,
                       TestUpdateDeviceAddedTimeDictOverwrite) {
  base::ScopedMockTimeMessageLoopTaskRunner mocked_task_runner;

  MediaSinkInternal cast_sink = CreateCastSink(1);

  UpdateDeviceAddedTimeDict(cast_sink.id());
  auto devices_added_time_dict = GetDeviceAddedTimeDictFromPrefService();
  auto initial_time_of_addition =
      base::ValueToTime(devices_added_time_dict.Find(cast_sink.id())).value();

  mocked_task_runner->FastForwardBy(base::Seconds(10));
  UpdateDeviceAddedTimeDict(cast_sink.id());
  devices_added_time_dict = GetDeviceAddedTimeDictFromPrefService();
  auto final_time_of_addition =
      base::ValueToTime(devices_added_time_dict.Find(cast_sink.id())).value();

  // Expect the two times of addition to be different, and the second time to be
  // greater.
  EXPECT_GE(final_time_of_addition, initial_time_of_addition);
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastPrefUpdaterLacrosTest,
                       TestGetMediaSinkInternalValueBySinkId) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  UpdateDevicesDict(cast_sink);
  base::test::TestFuture<base::Value::Dict> cast_sink_from_dict;
  pref_updater()->GetMediaSinkInternalValueBySinkId(
      cast_sink.id(), cast_sink_from_dict.GetCallback());
  EXPECT_EQ(CreateValueDictFromMediaSinkInternal(cast_sink),
            cast_sink_from_dict.Get());

  base::test::TestFuture<base::Value::Dict> cast_sink_from_dict2;
  pref_updater()->GetMediaSinkInternalValueBySinkId(
      cast_sink2.id(), cast_sink_from_dict2.GetCallback());
  EXPECT_TRUE(cast_sink_from_dict2.Get().empty());
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastPrefUpdaterLacrosTest,
                       TestRemoveSinkIdFromDevicesDict) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  UpdateDevicesDict(cast_sink);

  RemoveSinkIdFromDevicesDict(cast_sink.id());
  EXPECT_FALSE(GetDevicesDictFromPrefService().FindDict(cast_sink.id()));

  RemoveSinkIdFromDevicesDict(cast_sink.id());
  EXPECT_FALSE(GetDevicesDictFromPrefService().FindDict(cast_sink2.id()));
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastPrefUpdaterLacrosTest,
                       TestRemoveSinkIdFromDeviceAddedTimeDict) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  UpdateDeviceAddedTimeDict(cast_sink.id());

  RemoveSinkIdFromDeviceAddedTimeDict(cast_sink.id());
  EXPECT_FALSE(
      GetDeviceAddedTimeDictFromPrefService().contains(cast_sink.id()));

  RemoveSinkIdFromDeviceAddedTimeDict(cast_sink2.id());
  EXPECT_FALSE(
      GetDeviceAddedTimeDictFromPrefService().contains(cast_sink2.id()));
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastPrefUpdaterLacrosTest,
                       TestGetDeviceAddedTime) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  UpdateDeviceAddedTimeDict(cast_sink.id());
  auto devices_added_time_dict = GetDeviceAddedTimeDictFromPrefService();
  EXPECT_TRUE(devices_added_time_dict.contains(cast_sink.id()));
  EXPECT_FALSE(devices_added_time_dict.contains(cast_sink2.id()));
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastPrefUpdaterLacrosTest,
                       TestClearDevicesDict) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  UpdateDevicesDict(cast_sink);
  UpdateDevicesDict(cast_sink2);
  EXPECT_FALSE(GetDevicesDictFromPrefService().empty());

  base::RunLoop loop;
  pref_updater()->ClearDevicesDict(loop.QuitClosure());
  loop.Run();
  EXPECT_TRUE(GetDevicesDictFromPrefService().empty());
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastPrefUpdaterLacrosTest,
                       TestClearDeviceAddedTimeDict) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  UpdateDeviceAddedTimeDict(cast_sink.id());
  UpdateDeviceAddedTimeDict(cast_sink2.id());
  EXPECT_FALSE(GetDeviceAddedTimeDictFromPrefService().empty());

  base::RunLoop loop;
  pref_updater()->ClearDeviceAddedTimeDict(loop.QuitClosure());
  loop.Run();
  EXPECT_TRUE(GetDeviceAddedTimeDictFromPrefService().empty());
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastPrefUpdaterLacrosTest,
                       TestUpdateDevicesDictIdenticalIPs) {
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
  base::Value::Dict devices_dict = GetDevicesDictFromPrefService();
  EXPECT_EQ(devices_dict.size(), 2u);
  EXPECT_TRUE(devices_dict.contains(cast_sink2.id()));
  EXPECT_TRUE(devices_dict.contains(cast_sink3.id()));
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastPrefUpdaterLacrosTest,
                       TestUpdateDevicesDictDifferentIPs) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);
  MediaSinkInternal cast_sink3 = CreateCastSink(3);

  UpdateDevicesDict(cast_sink);
  UpdateDevicesDict(cast_sink2);
  UpdateDevicesDict(cast_sink3);

  // There should only be two devices stored since two ip_endpoints were
  // identical.
  base::Value::Dict devices_dict = GetDevicesDictFromPrefService();
  EXPECT_EQ(devices_dict.size(), 3u);
  EXPECT_TRUE(devices_dict.contains(cast_sink.id()));
  EXPECT_TRUE(devices_dict.contains(cast_sink2.id()));
  EXPECT_TRUE(devices_dict.contains(cast_sink3.id()));
}

}  // namespace media_router
