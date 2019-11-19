// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_media_controller.h"

#include "base/json/json_reader.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_record.h"
#include "chrome/browser/media/router/providers/cast/mock_activity_record.h"
#include "chrome/browser/media/router/test/media_router_mojo_test.h"
#include "chrome/common/media_router/media_route.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Value;
using testing::_;
using testing::Invoke;
using testing::WithArg;

namespace media_router {

namespace {

constexpr char kSessionId[] = "sessionId123";
constexpr int kMediaSessionId = 12345678;

// Verifies that the session ID is |kSessionId|.
void VerifySessionId(const Value& v2_message_body) {
  const Value* sessionId = v2_message_body.FindKey("sessionId");
  ASSERT_TRUE(sessionId);
  ASSERT_TRUE(sessionId->is_string());
  EXPECT_EQ(kSessionId, sessionId->GetString());
}

// Verifies that the media session ID is |kMediaSessionId|.
void VerifySessionAndMediaSessionIds(const Value& v2_message_body) {
  VerifySessionId(v2_message_body);
  const Value* mediaSessionId = v2_message_body.FindKey("mediaSessionId");
  ASSERT_TRUE(mediaSessionId);
  ASSERT_TRUE(mediaSessionId->is_int());
  EXPECT_EQ(kMediaSessionId, mediaSessionId->GetInt());
}

Value GetPlayerStateValue(const mojom::MediaStatus& status) {
  switch (status.play_state) {
    case mojom::MediaStatus::PlayState::PLAYING:
      return Value("PLAYING");
    case mojom::MediaStatus::PlayState::PAUSED:
      return Value("PAUSED");
    case mojom::MediaStatus::PlayState::BUFFERING:
      return Value("BUFFERING");
  }
}

Value GetSupportedMediaCommandsValue(const mojom::MediaStatus& status) {
  int commands = 0;
  // |can_set_volume| and |can_mute| are not used, because the receiver volume
  // is used instead.
  if (status.can_play_pause)
    commands |= 1;
  if (status.can_seek)
    commands |= 2;
  return Value(commands);
}

Value CreateImagesValue(const std::vector<mojom::MediaImagePtr>& images) {
  Value image_list(Value::Type::LIST);
  for (const mojom::MediaImagePtr& image : images) {
    Value image_value(Value::Type::DICTIONARY);
    image_value.SetStringKey("url", image->url.spec());
    // CastMediaController should be able to handle images that are missing the
    // width or the height.
    if (image->size) {
      image_value.SetIntKey("width", image->size->width());
      image_value.SetIntKey("height", image->size->height());
    }
    image_list.Append(std::move(image_value));
  }
  return image_list;
}

mojom::MediaStatusPtr CreateSampleMediaStatus() {
  mojom::MediaStatusPtr status = mojom::MediaStatus::New();
  status->title = "media title";
  status->can_play_pause = true;
  status->can_mute = true;
  status->can_set_volume = false;
  status->can_seek = false;
  status->is_muted = false;
  status->volume = 0.7;
  status->play_state = mojom::MediaStatus::PlayState::BUFFERING;
  status->duration = base::TimeDelta::FromSeconds(30);
  status->current_time = base::TimeDelta::FromSeconds(12);
  return status;
}

std::unique_ptr<CastSession> CreateSampleSession() {
  MediaSinkInternal sink(MediaSink("sinkId123", "name", SinkIconType::CAST),
                         CastSinkExtraData());
  base::Optional<Value> receiver_status = base::JSONReader::Read(R"({
    "applications": [{
      "appId": "ABCD1234",
      "displayName": "My App",
      "sessionId": "sessionId123",
      "transportId": "transportId123",
      "namespaces": [{"name": "urn:x-cast:com.example"}]
    }],
    "volume": {
      "controlType": "attenuation",
      "level": 0.8,
      "muted": false,
      "stepInterval": 0.1
    }
  })");
  return CastSession::From(sink, receiver_status.value());
}

}  // namespace

class CastMediaControllerTest : public testing::Test {
 public:
  CastMediaControllerTest() : activity_(MediaRoute(), "appId123") {}
  ~CastMediaControllerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    mojo::PendingRemote<mojom::MediaStatusObserver> mojo_status_observer;
    status_observer_ = std::make_unique<MockMediaStatusObserver>(
        mojo_status_observer.InitWithNewPipeAndPassReceiver());
    controller_ = std::make_unique<CastMediaController>(
        &activity_, mojo_controller_.BindNewPipeAndPassReceiver(),
        std::move(mojo_status_observer));
  }

  void TearDown() override {
    VerifyAndClearExpectations();
    testing::Test::TearDown();
  }

  void VerifyAndClearExpectations() {
    base::RunLoop().RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&activity_);
    testing::Mock::VerifyAndClearExpectations(status_observer_.get());
  }

  void SetSessionAndMediaStatus() {
    controller_->SetSession(*CreateSampleSession());
    SetMediaStatus(*CreateSampleMediaStatus());
  }

  void SetMediaStatus(const mojom::MediaStatus& status) {
    Value status_value(Value::Type::DICTIONARY);
    status_value.SetKey("mediaSessionId", Value(kMediaSessionId));
    status_value.SetKey("media", Value(Value::Type::DICTIONARY));
    status_value.SetPath("media.metadata", Value(Value::Type::DICTIONARY));
    status_value.SetPath("media.metadata.title", Value(status.title));
    status_value.SetPath("media.metadata.images",
                         CreateImagesValue(status.images));
    status_value.SetPath("media.duration", Value(status.duration.InSecondsF()));
    status_value.SetPath("currentTime",
                         Value(status.current_time.InSecondsF()));
    status_value.SetPath("playerState", GetPlayerStateValue(status));
    status_value.SetPath("supportedMediaCommands",
                         GetSupportedMediaCommandsValue(status));
    status_value.SetPath("volume", Value(Value::Type::DICTIONARY));
    status_value.SetPath("volume.level", Value(status.volume));
    status_value.SetPath("volume.muted", Value(status.is_muted));

    Value status_list(Value::Type::DICTIONARY);
    status_list.SetKey("status", Value(Value::Type::LIST));
    status_list.FindKey("status")->Append(std::move(status_value));
    controller_->SetMediaStatus(std::move(status_list));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  MockActivityRecord activity_;
  std::unique_ptr<CastMediaController> controller_;
  mojo::Remote<mojom::MediaController> mojo_controller_;
  std::unique_ptr<MockMediaStatusObserver> status_observer_;
};

TEST_F(CastMediaControllerTest, SendPlayRequest) {
  SetSessionAndMediaStatus();
  EXPECT_CALL(activity_, SendMediaRequestToReceiver(_))
      .WillOnce([](const CastInternalMessage& cast_message) {
        EXPECT_EQ("PLAY", cast_message.v2_message_type());
        VerifySessionAndMediaSessionIds(cast_message.v2_message_body());
        return 0;
      });
  mojo_controller_->Play();
}

TEST_F(CastMediaControllerTest, SendPauseRequest) {
  SetSessionAndMediaStatus();
  EXPECT_CALL(activity_, SendMediaRequestToReceiver(_))
      .WillOnce([](const CastInternalMessage& cast_message) {
        EXPECT_EQ("PAUSE", cast_message.v2_message_type());
        VerifySessionAndMediaSessionIds(cast_message.v2_message_body());
        return 0;
      });
  mojo_controller_->Pause();
}

TEST_F(CastMediaControllerTest, SendMuteRequests) {
  SetSessionAndMediaStatus();
  EXPECT_CALL(activity_, SendSetVolumeRequestToReceiver(_, _))
      .WillOnce(WithArg<0>([](const CastInternalMessage& cast_message) {
        EXPECT_EQ("SET_VOLUME", cast_message.v2_message_type());
        EXPECT_TRUE(
            cast_message.v2_message_body().FindPath("volume.muted")->GetBool());
        VerifySessionId(cast_message.v2_message_body());
        return 0;
      }));
  mojo_controller_->SetMute(true);
  VerifyAndClearExpectations();

  EXPECT_CALL(activity_, SendSetVolumeRequestToReceiver(_, _))
      .WillOnce(WithArg<0>([](const CastInternalMessage& cast_message) {
        EXPECT_EQ("SET_VOLUME", cast_message.v2_message_type());
        EXPECT_FALSE(
            cast_message.v2_message_body().FindPath("volume.muted")->GetBool());
        VerifySessionId(cast_message.v2_message_body());
        return 0;
      }));
  mojo_controller_->SetMute(false);
}

TEST_F(CastMediaControllerTest, SendVolumeRequest) {
  SetSessionAndMediaStatus();
  EXPECT_CALL(activity_, SendSetVolumeRequestToReceiver(_, _))
      .WillOnce(WithArg<0>([&](const CastInternalMessage& cast_message) {
        EXPECT_EQ("SET_VOLUME", cast_message.v2_message_type());
        EXPECT_FLOAT_EQ(0.314, cast_message.v2_message_body()
                                   .FindPath("volume.level")
                                   ->GetDouble());
        VerifySessionId(cast_message.v2_message_body());
        return 0;
      }));
  mojo_controller_->SetVolume(0.314);
}

TEST_F(CastMediaControllerTest, SendSeekRequest) {
  SetSessionAndMediaStatus();
  EXPECT_CALL(activity_, SendMediaRequestToReceiver(_))
      .WillOnce([&](const CastInternalMessage& cast_message) {
        EXPECT_EQ("SEEK", cast_message.v2_message_type());
        EXPECT_DOUBLE_EQ(
            12.34,
            cast_message.v2_message_body().FindKey("currentTime")->GetDouble());
        VerifySessionId(cast_message.v2_message_body());
        return 0;
      });
  mojo_controller_->Seek(base::TimeDelta::FromSecondsD(12.34));
}

TEST_F(CastMediaControllerTest, SendNextTrackRequest) {
  SetSessionAndMediaStatus();
  EXPECT_CALL(activity_, SendMediaRequestToReceiver(_))
      .WillOnce([](const CastInternalMessage& cast_message) {
        EXPECT_EQ("QUEUE_NEXT", cast_message.v2_message_type());
        VerifySessionAndMediaSessionIds(cast_message.v2_message_body());
        return 0;
      });
  mojo_controller_->NextTrack();
}

TEST_F(CastMediaControllerTest, SendPreviousTrackRequest) {
  SetSessionAndMediaStatus();
  EXPECT_CALL(activity_, SendMediaRequestToReceiver(_))
      .WillOnce([](const CastInternalMessage& cast_message) {
        EXPECT_EQ("QUEUE_PREV", cast_message.v2_message_type());
        VerifySessionAndMediaSessionIds(cast_message.v2_message_body());
        return 0;
      });
  mojo_controller_->PreviousTrack();
}

TEST_F(CastMediaControllerTest, UpdateMediaStatus) {
  mojom::MediaStatusPtr expected_status = CreateSampleMediaStatus();

  EXPECT_CALL(*status_observer_, OnMediaStatusUpdated(_))
      .WillOnce([&](mojom::MediaStatusPtr status) {
        EXPECT_EQ(expected_status->title, status->title);
        EXPECT_EQ(expected_status->can_play_pause, status->can_play_pause);
        EXPECT_EQ(expected_status->play_state, status->play_state);
        EXPECT_EQ(expected_status->duration, status->duration);
        EXPECT_EQ(expected_status->current_time, status->current_time);
      });
  SetMediaStatus(*expected_status);
  VerifyAndClearExpectations();
}

TEST_F(CastMediaControllerTest, UpdateMediaImages) {
  mojom::MediaStatusPtr expected_status = CreateSampleMediaStatus();
  expected_status->images.emplace_back(
      base::in_place, GURL("https://example.com/1.png"), gfx::Size(123, 456));
  expected_status->images.emplace_back(
      base::in_place, GURL("https://example.com/2.png"), gfx::Size(789, 0));
  const mojom::MediaImage& image1 = *expected_status->images.at(0);
  const mojom::MediaImage& image2 = *expected_status->images.at(1);

  EXPECT_CALL(*status_observer_, OnMediaStatusUpdated(_))
      .WillOnce([&](const mojom::MediaStatusPtr& status) {
        ASSERT_EQ(2u, status->images.size());
        EXPECT_EQ(image1.url.spec(), status->images.at(0)->url.spec());
        EXPECT_EQ(image1.size->width(), status->images.at(0)->size->width());
        EXPECT_EQ(image1.size->height(), status->images.at(0)->size->height());
        EXPECT_EQ(image2.url.spec(), status->images.at(1)->url.spec());
        EXPECT_EQ(base::nullopt, status->images.at(1)->size);
      });
  SetMediaStatus(*expected_status);
  VerifyAndClearExpectations();
}

TEST_F(CastMediaControllerTest, UpdateVolumeStatus) {
  auto session = CreateSampleSession();
  const float session_volume =
      session->value().FindPath("receiver.volume.level")->GetDouble();
  const bool session_muted =
      session->value().FindPath("receiver.volume.muted")->GetBool();
  EXPECT_CALL(*status_observer_, OnMediaStatusUpdated(_))
      .WillOnce([&](mojom::MediaStatusPtr status) {
        EXPECT_FLOAT_EQ(session_volume, status->volume);
        EXPECT_EQ(session_muted, status->is_muted);
      });
  controller_->SetSession(*session);
  VerifyAndClearExpectations();

  // The volume info is set in SetSession() rather than SetMediaStatus(), so the
  // volume info in the latter should be ignored.
  EXPECT_CALL(*status_observer_, OnMediaStatusUpdated(_))
      .WillOnce([&](mojom::MediaStatusPtr status) {
        EXPECT_FLOAT_EQ(session_volume, status->volume);
        EXPECT_EQ(session_muted, status->is_muted);
      });
  mojom::MediaStatusPtr updated_status = CreateSampleMediaStatus();
  updated_status->volume = 0.3;
  updated_status->is_muted = true;
  SetMediaStatus(*updated_status);
  VerifyAndClearExpectations();
}

}  // namespace media_router
