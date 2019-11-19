// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_app_discovery_service.h"

#include "base/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/common/media_router/discovery/media_sink_service_base.h"
#include "chrome/common/media_router/providers/cast/cast_media_source.h"
#include "chrome/common/media_router/test/test_helper.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using cast_channel::GetAppAvailabilityResult;
using testing::_;
using testing::Invoke;

namespace media_router {

class CastAppDiscoveryServiceTest : public testing::Test {
 public:
  CastAppDiscoveryServiceTest()
      : task_runner_(base::MakeRefCounted<base::TestSimpleTaskRunner>()),
        socket_service_(task_runner_),
        message_handler_(&socket_service_),
        app_discovery_service_(
            std::make_unique<CastAppDiscoveryServiceImpl>(&message_handler_,
                                                          &socket_service_,
                                                          &media_sink_service_,
                                                          &clock_)),
        source_a_1_(
            *CastMediaSource::FromMediaSourceId("cast:AAAAAAAA?clientId=1")),
        source_a_2_(
            *CastMediaSource::FromMediaSourceId("cast:AAAAAAAA?clientId=2")),
        source_b_1_(
            *CastMediaSource::FromMediaSourceId("cast:BBBBBBBB?clientId=1")) {
    ON_CALL(socket_service_, GetSocket(_))
        .WillByDefault(testing::Return(&socket_));
    task_runner_->RunPendingTasks();
  }

  ~CastAppDiscoveryServiceTest() override { task_runner_->RunPendingTasks(); }

  MOCK_METHOD2(OnSinkQueryUpdated,
               void(const MediaSource::Id&,
                    const std::vector<MediaSinkInternal>&));

  void AddOrUpdateSink(const MediaSinkInternal& sink) {
    media_sink_service_.AddOrUpdateSink(sink);
  }

  void RemoveSink(const MediaSinkInternal& sink) {
    media_sink_service_.RemoveSink(sink);
  }

  CastAppDiscoveryService::Subscription StartObservingMediaSinksInitially(
      const CastMediaSource& source) {
    auto subscription = app_discovery_service_->StartObservingMediaSinks(
        source,
        base::BindRepeating(&CastAppDiscoveryServiceTest::OnSinkQueryUpdated,
                            base::Unretained(this)));
    task_runner_->RunPendingTasks();
    return subscription;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SimpleTestTickClock clock_;
  testing::NiceMock<cast_channel::MockCastSocketService> socket_service_;
  cast_channel::MockCastSocket socket_;
  cast_channel::MockCastMessageHandler message_handler_;
  TestMediaSinkService media_sink_service_;
  std::unique_ptr<CastAppDiscoveryService> app_discovery_service_;
  CastMediaSource source_a_1_;
  CastMediaSource source_a_2_;
  CastMediaSource source_b_1_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastAppDiscoveryServiceTest);
};

TEST_F(CastAppDiscoveryServiceTest, StartObservingMediaSinks) {
  auto subscription1 = StartObservingMediaSinksInitially(source_a_1_);

  // Adding a sink after app registered causes app availability request to be
  // sent.
  MediaSinkInternal sink1 = CreateCastSink(1);
  cast_channel::GetAppAvailabilityCallback cb;
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "AAAAAAAA", _))
      .WillOnce([&cb](cast_channel::CastSocket*, const std::string&,
                      cast_channel::GetAppAvailabilityCallback callback) {
        cb = std::move(callback);
      });

  AddOrUpdateSink(sink1);

  // Same app ID should not trigger another request.
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, _, _)).Times(0);
  auto subscription2 = app_discovery_service_->StartObservingMediaSinks(
      source_a_2_,
      base::BindRepeating(&CastAppDiscoveryServiceTest::OnSinkQueryUpdated,
                          base::Unretained(this)));

  std::vector<MediaSinkInternal> sinks_1 = {sink1};
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_1_.source_id(), sinks_1));
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_2_.source_id(), sinks_1));
  std::move(cb).Run("AAAAAAAA", GetAppAvailabilityResult::kAvailable);

  // No more updates for |source_a_1_|.
  subscription1.reset();
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_1_.source_id(), _)).Times(0);
  EXPECT_CALL(*this,
              OnSinkQueryUpdated(source_a_2_.source_id(), testing::IsEmpty()));
  RemoveSink(sink1);
}

TEST_F(CastAppDiscoveryServiceTest, ReAddSinkQueryUsesCachedValue) {
  auto subscription1 = StartObservingMediaSinksInitially(source_a_1_);

  // Adding a sink after app registered causes app availability request to be
  // sent.
  MediaSinkInternal sink1 = CreateCastSink(1);
  cast_channel::GetAppAvailabilityCallback cb;
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "AAAAAAAA", _))
      .WillOnce([&cb](cast_channel::CastSocket*, const std::string&,
                      cast_channel::GetAppAvailabilityCallback callback) {
        cb = std::move(callback);
      });

  AddOrUpdateSink(sink1);

  std::vector<MediaSinkInternal> sinks_1 = {sink1};
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_1_.source_id(), sinks_1));
  std::move(cb).Run("AAAAAAAA", GetAppAvailabilityResult::kAvailable);

  subscription1.reset();

  // Request not re-sent; cached kAvailable value is used.
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, _, _)).Times(0);
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_1_.source_id(), sinks_1));
  subscription1 = StartObservingMediaSinksInitially(source_a_1_);
}

TEST_F(CastAppDiscoveryServiceTest, SinkQueryUpdatedOnSinkUpdate) {
  auto subscription1 = StartObservingMediaSinksInitially(source_a_1_);

  // Adding a sink after app registered causes app availability request to be
  // sent.
  MediaSinkInternal sink1 = CreateCastSink(1);
  cast_channel::GetAppAvailabilityCallback cb;
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "AAAAAAAA", _))
      .WillOnce([&cb](cast_channel::CastSocket*, const std::string&,
                      cast_channel::GetAppAvailabilityCallback callback) {
        cb = std::move(callback);
      });

  AddOrUpdateSink(sink1);

  // Query now includes |sink1|.
  std::vector<MediaSinkInternal> sinks_1 = {sink1};
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_1_.source_id(), sinks_1));
  std::move(cb).Run("AAAAAAAA", GetAppAvailabilityResult::kAvailable);

  // Updating |sink1| causes |source_a_1_| query to be updated.
  sink1.sink().set_name("Updated name");
  sinks_1 = {sink1};
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_1_.source_id(), sinks_1));
  AddOrUpdateSink(sink1);
}

TEST_F(CastAppDiscoveryServiceTest, Refresh) {
  auto subscription1 = StartObservingMediaSinksInitially(source_a_1_);
  auto subscription2 = StartObservingMediaSinksInitially(source_b_1_);

  MediaSinkInternal sink1 = CreateCastSink(1);
  EXPECT_CALL(*this, OnSinkQueryUpdated(_, _));
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "AAAAAAAA", _))
      .WillOnce([](cast_channel::CastSocket*, const std::string& app_id,
                   cast_channel::GetAppAvailabilityCallback callback) {
        std::move(callback).Run(app_id, GetAppAvailabilityResult::kAvailable);
      });
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "BBBBBBBB", _))
      .WillOnce([](cast_channel::CastSocket*, const std::string& app_id,
                   cast_channel::GetAppAvailabilityCallback callback) {
        std::move(callback).Run(app_id, GetAppAvailabilityResult::kUnknown);
      });
  AddOrUpdateSink(sink1);

  MediaSinkInternal sink2 = CreateCastSink(2);
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "AAAAAAAA", _))
      .WillOnce([](cast_channel::CastSocket*, const std::string& app_id,
                   cast_channel::GetAppAvailabilityCallback callback) {
        std::move(callback).Run(app_id, GetAppAvailabilityResult::kUnavailable);
      });
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "BBBBBBBB", _));
  AddOrUpdateSink(sink2);

  clock_.Advance(base::TimeDelta::FromSeconds(30));

  // Request app availability for app B for both sinks.
  // App A on |sink2| is not requested due to timing threshold.
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "AAAAAAAA", _))
      .Times(0);
  EXPECT_CALL(*this, OnSinkQueryUpdated(_, _)).Times(2);
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "BBBBBBBB", _))
      .Times(2)
      .WillRepeatedly([](cast_channel::CastSocket*, const std::string& app_id,
                         cast_channel::GetAppAvailabilityCallback callback) {
        std::move(callback).Run(app_id, GetAppAvailabilityResult::kAvailable);
      });
  app_discovery_service_->Refresh();

  clock_.Advance(base::TimeDelta::FromSeconds(31));

  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "AAAAAAAA", _));
  app_discovery_service_->Refresh();
}

TEST_F(CastAppDiscoveryServiceTest, StartObservingMediaSinksAfterSinkAdded) {
  // No registered apps.
  MediaSinkInternal sink1 = CreateCastSink(1);
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, _, _)).Times(0);
  AddOrUpdateSink(sink1);

  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "AAAAAAAA", _));
  auto subscription1 = app_discovery_service_->StartObservingMediaSinks(
      source_a_1_,
      base::BindRepeating(&CastAppDiscoveryServiceTest::OnSinkQueryUpdated,
                          base::Unretained(this)));

  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "BBBBBBBB", _));
  auto subscription2 = app_discovery_service_->StartObservingMediaSinks(
      source_b_1_,
      base::BindRepeating(&CastAppDiscoveryServiceTest::OnSinkQueryUpdated,
                          base::Unretained(this)));

  // Adding new sink causes availability requests for 2 apps to be sent to the
  // new sink.
  MediaSinkInternal sink2 = CreateCastSink(2);
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "AAAAAAAA", _));
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "BBBBBBBB", _));
  AddOrUpdateSink(sink2);
}

TEST_F(CastAppDiscoveryServiceTest, StartObservingMediaSinksCachedValue) {
  auto subscription1 = StartObservingMediaSinksInitially(source_a_1_);

  // Adding a sink after app registered causes app availability request to be
  // sent.
  MediaSinkInternal sink1 = CreateCastSink(1);
  cast_channel::GetAppAvailabilityCallback cb;
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "AAAAAAAA", _))
      .WillOnce([&cb](cast_channel::CastSocket*, const std::string&,
                      cast_channel::GetAppAvailabilityCallback callback) {
        cb = std::move(callback);
      });
  AddOrUpdateSink(sink1);

  std::vector<MediaSinkInternal> sinks_1 = {sink1};
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_1_.source_id(), sinks_1));
  std::move(cb).Run("AAAAAAAA", GetAppAvailabilityResult::kAvailable);

  // Same app ID should not trigger another request, but it should return
  // cached value.
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, _, _)).Times(0);
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_2_.source_id(), sinks_1));
  auto subscription2 = app_discovery_service_->StartObservingMediaSinks(
      source_a_2_,
      base::BindRepeating(&CastAppDiscoveryServiceTest::OnSinkQueryUpdated,
                          base::Unretained(this)));

  // Same source as |source_a_1_|. The callback will be invoked.
  auto source3 = CastMediaSource::FromMediaSourceId("cast:AAAAAAAA?clientId=1");
  ASSERT_TRUE(source3);
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, _, _)).Times(0);
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_1_.source_id(), sinks_1));
  auto subscription3 = app_discovery_service_->StartObservingMediaSinks(
      *source3,
      base::BindRepeating(&CastAppDiscoveryServiceTest::OnSinkQueryUpdated,
                          base::Unretained(this)));
}

TEST_F(CastAppDiscoveryServiceTest, AvailabilityUnknownOrUnavailable) {
  auto subscription1 = StartObservingMediaSinksInitially(source_a_1_);

  // Adding a sink after app registered causes app availability request to be
  // sent.
  MediaSinkInternal sink1 = CreateCastSink(1);
  EXPECT_CALL(*this, OnSinkQueryUpdated(_, _)).Times(0);
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "AAAAAAAA", _))
      .WillOnce([](cast_channel::CastSocket*, const std::string&,
                   cast_channel::GetAppAvailabilityCallback callback) {
        std::move(callback).Run("AAAAAAAA", GetAppAvailabilityResult::kUnknown);
      });
  AddOrUpdateSink(sink1);

  // Sink updated and unknown app availability will cause request to be sent
  // again.
  EXPECT_CALL(*this, OnSinkQueryUpdated(_, _)).Times(0);
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "AAAAAAAA", _))
      .WillOnce([](cast_channel::CastSocket*, const std::string&,
                   cast_channel::GetAppAvailabilityCallback callback) {
        std::move(callback).Run("AAAAAAAA",
                                GetAppAvailabilityResult::kUnavailable);
      });
  AddOrUpdateSink(sink1);

  // Known availability -- no request sent.
  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "AAAAAAAA", _))
      .Times(0);
  AddOrUpdateSink(sink1);

  // Removing the sink will also remove previous availability information.
  // Next time sink is added, request will be sent.
  EXPECT_CALL(*this, OnSinkQueryUpdated(_, _)).Times(0);
  RemoveSink(sink1);

  EXPECT_CALL(message_handler_, RequestAppAvailability(_, "AAAAAAAA", _));
  AddOrUpdateSink(sink1);
}

}  // namespace media_router
