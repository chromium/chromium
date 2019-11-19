// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"

#include "base/bind.h"
#include "base/test/mock_callback.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/media/router/discovery/dial/dial_registry.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::Return;

namespace media_router {

class TestDialRegistry : public DialRegistry {
 public:
  TestDialRegistry() {}
  ~TestDialRegistry() override {}

  MOCK_METHOD1(RegisterObserver, void(DialRegistry::Observer* observer));
  MOCK_METHOD1(UnregisterObserver, void(DialRegistry::Observer* observer));

  MOCK_METHOD0(OnListenerAdded, void());
  MOCK_METHOD0(OnListenerRemoved, void());
};

class MockDeviceDescriptionService : public DeviceDescriptionService {
 public:
  MockDeviceDescriptionService(DeviceDescriptionParseSuccessCallback success_cb,
                               DeviceDescriptionParseErrorCallback error_cb)
      : DeviceDescriptionService(success_cb, error_cb) {}
  ~MockDeviceDescriptionService() override {}

  MOCK_METHOD1(GetDeviceDescriptions,
               void(const std::vector<DialDeviceData>& devices));
};

class DialMediaSinkServiceImplTest : public ::testing::Test {
 public:
  DialMediaSinkServiceImplTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        media_sink_service_(new DialMediaSinkServiceImpl(
            mock_sink_discovered_cb_.Get(),
            base::SequencedTaskRunnerHandle::Get())) {}

  void SetUp() override {
    media_sink_service_->SetDialRegistryForTest(&test_dial_registry_);

    auto mock_description_service =
        std::make_unique<MockDeviceDescriptionService>(mock_success_cb_.Get(),
                                                       mock_error_cb_.Get());
    mock_description_service_ = mock_description_service.get();
    media_sink_service_->SetDescriptionServiceForTest(
        std::move(mock_description_service));

    mock_timer_ = new base::MockOneShotTimer();
    media_sink_service_->SetTimerForTest(base::WrapUnique(mock_timer_));

    auto mock_app_discovery_service =
        std::make_unique<MockDialAppDiscoveryService>();
    mock_app_discovery_service_ = mock_app_discovery_service.get();
    media_sink_service_->SetAppDiscoveryServiceForTest(
        std::move(mock_app_discovery_service));
    base::RunLoop().RunUntilIdle();
  }

  DialMediaSinkServiceImpl::SinkQueryByAppSubscription
  StartMonitoringAvailableSinksForApp(const std::string& app_name) {
    return media_sink_service_->StartMonitoringAvailableSinksForApp(
        app_name, base::BindRepeating(
                      &DialMediaSinkServiceImplTest::GetAvailableSinksForApp,
                      base::Unretained(this)));
  }

  void GetAvailableSinksForApp(const std::string& app_name) {
    OnSinksAvailableForApp(app_name,
                           media_sink_service_->GetAvailableSinks(app_name));
  }

  MOCK_METHOD2(OnSinksAvailableForApp,
               void(const std::string& app_name,
                    const std::vector<MediaSinkInternal>& available_sinks));

  DialAppInfoResult CreateDialAppInfoResult(const std::string& app_name) {
    return DialAppInfoResult(
        CreateParsedDialAppInfoPtr(app_name, DialAppState::kRunning),
        DialAppInfoResultCode::kOk);
  }

 protected:
  const content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  base::MockCallback<OnSinksDiscoveredCallback> mock_sink_discovered_cb_;
  base::MockCallback<
      MockDeviceDescriptionService::DeviceDescriptionParseSuccessCallback>
      mock_success_cb_;
  base::MockCallback<
      MockDeviceDescriptionService::DeviceDescriptionParseErrorCallback>
      mock_error_cb_;

  TestDialRegistry test_dial_registry_;
  MockDeviceDescriptionService* mock_description_service_;
  MockDialAppDiscoveryService* mock_app_discovery_service_;
  base::MockOneShotTimer* mock_timer_;

  std::unique_ptr<DialMediaSinkServiceImpl> media_sink_service_;

  MediaSinkInternal dial_sink_1_ = CreateDialSink(1);
  MediaSinkInternal dial_sink_2_ = CreateDialSink(2);

  DISALLOW_COPY_AND_ASSIGN(DialMediaSinkServiceImplTest);
};

TEST_F(DialMediaSinkServiceImplTest, OnDeviceDescriptionAvailable) {
  DialDeviceData device_data("first", GURL("http://127.0.0.1/dd.xml"),
                             base::Time::Now());
  ParsedDialDeviceDescription device_description;
  device_description.model_name = "model name";
  device_description.friendly_name = "friendly name";
  device_description.app_url = GURL("http://192.168.1.1/apps");
  device_description.unique_id = "unique id";

  media_sink_service_->OnDeviceDescriptionAvailable(device_data,
                                                    device_description);
  EXPECT_TRUE(media_sink_service_->GetSinks().empty());

  std::vector<DialDeviceData> device_list = {device_data};
  EXPECT_CALL(*mock_description_service_, GetDeviceDescriptions(device_list));

  media_sink_service_->OnDialDeviceEvent(device_list);
  media_sink_service_->OnDeviceDescriptionAvailable(device_data,
                                                    device_description);
  EXPECT_TRUE(mock_timer_->IsRunning());
  EXPECT_CALL(mock_sink_discovered_cb_, Run(Not(IsEmpty())));
  mock_timer_->Fire();
  EXPECT_EQ(1u, media_sink_service_->GetSinks().size());
}

TEST_F(DialMediaSinkServiceImplTest,
       OnDeviceDescriptionAvailableIPAddressChanged) {
  DialDeviceData device_data("first", GURL("http://127.0.0.1/dd.xml"),
                             base::Time::Now());
  ParsedDialDeviceDescription device_description;
  device_description.model_name = "model name";
  device_description.friendly_name = "friendly name";
  device_description.app_url = GURL("http://192.168.1.1/apps");
  device_description.unique_id = "unique id";

  std::vector<DialDeviceData> device_list = {device_data};
  EXPECT_CALL(*mock_description_service_, GetDeviceDescriptions(device_list));
  media_sink_service_->OnDialDeviceEvent(device_list);

  media_sink_service_->OnDeviceDescriptionAvailable(device_data,
                                                    device_description);

  EXPECT_TRUE(mock_timer_->IsRunning());
  EXPECT_CALL(mock_sink_discovered_cb_, Run(_));
  mock_timer_->Fire();
  EXPECT_EQ(1u, media_sink_service_->GetSinks().size());

  device_description.app_url = GURL("http://192.168.1.100/apps");
  media_sink_service_->OnDeviceDescriptionAvailable(device_data,
                                                    device_description);

  EXPECT_TRUE(mock_timer_->IsRunning());
  EXPECT_CALL(mock_sink_discovered_cb_, Run(_));
  mock_timer_->Fire();

  EXPECT_EQ(1u, media_sink_service_->GetSinks().size());
  for (const auto& dial_sink_it : media_sink_service_->GetSinks()) {
    EXPECT_EQ(device_description.app_url,
              dial_sink_it.second.dial_data().app_url);
  }
}

TEST_F(DialMediaSinkServiceImplTest, OnDeviceDescriptionRestartsTimer) {
  DialDeviceData device_data("first", GURL("http://127.0.0.1/dd.xml"),
                             base::Time::Now());
  ParsedDialDeviceDescription device_description;
  device_description.model_name = "model name";
  device_description.friendly_name = "friendly name";
  device_description.app_url = GURL("http://192.168.1.1/apps");
  device_description.unique_id = "unique id";

  std::vector<DialDeviceData> device_list = {device_data};
  EXPECT_CALL(*mock_description_service_, GetDeviceDescriptions(device_list));

  EXPECT_FALSE(mock_timer_->IsRunning());
  media_sink_service_->OnDialDeviceEvent(device_list);
  media_sink_service_->OnDeviceDescriptionAvailable(device_data,
                                                    device_description);
  EXPECT_TRUE(mock_timer_->IsRunning());

  EXPECT_CALL(mock_sink_discovered_cb_, Run(_));
  mock_timer_->Fire();

  EXPECT_FALSE(mock_timer_->IsRunning());
  device_description.app_url = GURL("http://192.168.1.11/apps");
  media_sink_service_->OnDeviceDescriptionAvailable(device_data,
                                                    device_description);
  EXPECT_TRUE(mock_timer_->IsRunning());
}

TEST_F(DialMediaSinkServiceImplTest, OnDialDeviceEventRestartsTimer) {
  EXPECT_CALL(*mock_description_service_, GetDeviceDescriptions(IsEmpty()));
  media_sink_service_->OnDialDeviceEvent(std::vector<DialDeviceData>());
  EXPECT_TRUE(mock_timer_->IsRunning());

  EXPECT_CALL(mock_sink_discovered_cb_, Run(_)).Times(0);
  mock_timer_->Fire();

  EXPECT_CALL(*mock_description_service_, GetDeviceDescriptions(IsEmpty()));
  media_sink_service_->OnDialDeviceEvent(std::vector<DialDeviceData>());
  EXPECT_TRUE(mock_timer_->IsRunning());

  EXPECT_CALL(mock_sink_discovered_cb_, Run(_)).Times(0);
  mock_timer_->Fire();
}

TEST_F(DialMediaSinkServiceImplTest, StartStopMonitoringAvailableSinksForApp) {
  const MediaSink::Id& sink_id = dial_sink_1_.sink().id();
  EXPECT_CALL(*mock_app_discovery_service_,
              DoFetchDialAppInfo(sink_id, "YouTube"))
      .Times(1);
  media_sink_service_->AddOrUpdateSink(dial_sink_1_);
  auto sub1 = StartMonitoringAvailableSinksForApp("YouTube");
  auto sub2 = StartMonitoringAvailableSinksForApp("YouTube");
  EXPECT_EQ(1u, media_sink_service_->sink_queries_.size());

  sub1.reset();
  EXPECT_EQ(1u, media_sink_service_->sink_queries_.size());
  sub2.reset();
  EXPECT_TRUE(media_sink_service_->sink_queries_.empty());
}

TEST_F(DialMediaSinkServiceImplTest, OnDialAppInfoAvailableNoStartMonitoring) {
  const MediaSink::Id& sink_id = dial_sink_1_.sink().id();

  EXPECT_CALL(*this, OnSinksAvailableForApp(_, _)).Times(0);
  media_sink_service_->AddOrUpdateSink(dial_sink_1_);
  media_sink_service_->OnAppInfoParseCompleted(
      sink_id, "YouTube", CreateDialAppInfoResult("YouTube"));
}

TEST_F(DialMediaSinkServiceImplTest, OnDialAppInfoAvailableNoSink) {
  const MediaSink::Id& sink_id = dial_sink_1_.sink().id();

  EXPECT_CALL(*this, OnSinksAvailableForApp("YouTube", _)).Times(0);
  auto sub = StartMonitoringAvailableSinksForApp("YouTube");
  media_sink_service_->OnAppInfoParseCompleted(
      sink_id, "YouTube", CreateDialAppInfoResult("YouTube"));
}

TEST_F(DialMediaSinkServiceImplTest, OnDialAppInfoAvailableSinksAdded) {
  const MediaSink::Id& sink_id1 = dial_sink_1_.sink().id();
  const MediaSink::Id& sink_id2 = dial_sink_2_.sink().id();

  media_sink_service_->AddOrUpdateSink(dial_sink_1_);
  media_sink_service_->AddOrUpdateSink(dial_sink_2_);

  EXPECT_CALL(*mock_app_discovery_service_,
              DoFetchDialAppInfo(sink_id1, "YouTube"));
  EXPECT_CALL(*mock_app_discovery_service_,
              DoFetchDialAppInfo(sink_id2, "YouTube"));
  EXPECT_CALL(*mock_app_discovery_service_,
              DoFetchDialAppInfo(sink_id1, "Netflix"));
  EXPECT_CALL(*mock_app_discovery_service_,
              DoFetchDialAppInfo(sink_id2, "Netflix"));
  EXPECT_CALL(*this, OnSinksAvailableForApp(_, _)).Times(0);
  auto sub1 = StartMonitoringAvailableSinksForApp("YouTube");
  auto sub2 = StartMonitoringAvailableSinksForApp("Netflix");

  // Either kStopped or kRunning means the app is available on the sink.
  EXPECT_CALL(*this,
              OnSinksAvailableForApp(
                  "YouTube", std::vector<MediaSinkInternal>({dial_sink_1_})));
  media_sink_service_->OnAppInfoParseCompleted(
      sink_id1, "YouTube", CreateDialAppInfoResult("YouTube"));

  EXPECT_CALL(*this, OnSinksAvailableForApp("YouTube",
                                            std::vector<MediaSinkInternal>(
                                                {dial_sink_1_, dial_sink_2_})));
  media_sink_service_->OnAppInfoParseCompleted(
      sink_id2, "YouTube", CreateDialAppInfoResult("YouTube"));

  EXPECT_CALL(*this,
              OnSinksAvailableForApp(
                  "Netflix", std::vector<MediaSinkInternal>({dial_sink_2_})));
  media_sink_service_->OnAppInfoParseCompleted(
      sink_id2, "Netflix", CreateDialAppInfoResult("Netflix"));

  // Stop listening for Netflix.
  sub2.reset();
  EXPECT_CALL(*this, OnSinksAvailableForApp("Netflix", _)).Times(0);
  media_sink_service_->OnAppInfoParseCompleted(
      sink_id1, "Netflix", CreateDialAppInfoResult("Netflix"));

  std::vector<MediaSinkInternal> expected_sinks = {dial_sink_1_, dial_sink_2_};
  EXPECT_EQ(expected_sinks, media_sink_service_->GetAvailableSinks("YouTube"));
  EXPECT_EQ(expected_sinks, media_sink_service_->GetAvailableSinks("Netflix"));
}

TEST_F(DialMediaSinkServiceImplTest, OnDialAppInfoAvailableSinksRemoved) {
  const MediaSink::Id& sink_id = dial_sink_1_.sink().id();

  EXPECT_CALL(*mock_app_discovery_service_, DoFetchDialAppInfo(_, _));
  media_sink_service_->AddOrUpdateSink(dial_sink_1_);
  auto sub1 = StartMonitoringAvailableSinksForApp("YouTube");

  EXPECT_CALL(*this,
              OnSinksAvailableForApp(
                  "YouTube", std::vector<MediaSinkInternal>({dial_sink_1_})));
  media_sink_service_->OnAppInfoParseCompleted(
      sink_id, "YouTube", CreateDialAppInfoResult("YouTube"));

  EXPECT_CALL(*this, OnSinksAvailableForApp("YouTube", IsEmpty()));
  media_sink_service_->RemoveSink(dial_sink_1_);
  media_sink_service_->OnDiscoveryComplete();
}

TEST_F(DialMediaSinkServiceImplTest,
       OnDialAppInfoAvailableWithAlreadyAvailableSinks) {
  const MediaSink::Id& sink_id = dial_sink_1_.sink().id();

  EXPECT_CALL(*mock_app_discovery_service_, DoFetchDialAppInfo(_, _));
  media_sink_service_->AddOrUpdateSink(dial_sink_1_);
  auto sub1 = StartMonitoringAvailableSinksForApp("YouTube");

  EXPECT_CALL(*this,
              OnSinksAvailableForApp(
                  "YouTube", std::vector<MediaSinkInternal>({dial_sink_1_})))
      .Times(1);
  media_sink_service_->OnAppInfoParseCompleted(
      sink_id, "YouTube", CreateDialAppInfoResult("YouTube"));
  media_sink_service_->OnAppInfoParseCompleted(
      sink_id, "YouTube", CreateDialAppInfoResult("YouTube"));
}

TEST_F(DialMediaSinkServiceImplTest, StartAfterStopMonitoringForApp) {
  EXPECT_CALL(*mock_app_discovery_service_, DoFetchDialAppInfo(_, _));
  media_sink_service_->AddOrUpdateSink(dial_sink_1_);
  auto sub1 = StartMonitoringAvailableSinksForApp("YouTube");
  std::vector<MediaSinkInternal> expected_sinks = {dial_sink_1_};

  EXPECT_CALL(*this, OnSinksAvailableForApp("YouTube", expected_sinks))
      .Times(1);
  media_sink_service_->OnAppInfoParseCompleted(
      dial_sink_1_.sink().id(), "YouTube", CreateDialAppInfoResult("YouTube"));

  sub1.reset();

  EXPECT_EQ(expected_sinks, media_sink_service_->GetAvailableSinks("YouTube"));
  auto sub2 = StartMonitoringAvailableSinksForApp("YouTube");
  EXPECT_EQ(expected_sinks, media_sink_service_->GetAvailableSinks("YouTube"));
}

TEST_F(DialMediaSinkServiceImplTest, FetchDialAppInfoWithDiscoveryOnlySink) {
  media_router::DialSinkExtraData extra_data = dial_sink_1_.dial_data();
  extra_data.model_name = "Eureka Dongle";
  dial_sink_1_.set_dial_data(extra_data);

  EXPECT_CALL(*mock_app_discovery_service_, DoFetchDialAppInfo(_, _)).Times(0);
  media_sink_service_->AddOrUpdateSink(dial_sink_1_);
  auto sub1 = StartMonitoringAvailableSinksForApp("YouTube");
}

}  // namespace media_router
