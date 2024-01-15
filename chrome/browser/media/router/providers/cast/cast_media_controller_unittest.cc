// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_media_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/media/router/providers/cast/app_activity.h"
#include "chrome/browser/media/router/providers/cast/mock_app_activity.h"
#include "chrome/browser/media/router/test/media_router_mojo_test.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/test/test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

using base::Value;
using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::WithArg;

namespace media_router {

namespace {

constexpr char kMediaTitle[] = "media title";
constexpr char kSessionId[] = "sessionId123";
constexpr int kMediaSessionId = 12345678;

// Verifies that the session ID is |kSessionId|.
void VerifySessionId(const Value::Dict& v2_message_body) {
  const Value* sessionId = v2_message_body.Find("sessionId");
  ASSERT_TRUE(sessionId);
  ASSERT_TRUE(sessionId->is_string());
  EXPECT_EQ(kSessionId, sessionId->GetString());
}

// Verifies that the media session ID is |kMediaSessionId|.
void VerifySessionAndMediaSessionIds(const Value::Dict& v2_message_body) {
  VerifySessionId(v2_message_body);
  const Value* mediaSessionId = v2_message_body.Find("mediaSessionId");
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

base::Value::List GetSupportedMediaCommandsValue(
    const mojom::MediaStatus& status) {
  base::Value::List commands;
  // |can_set_volume| and |can_mute| are not used, because the receiver volume
  // is used instead.
  if (status.can_play_pause)
    commands.Append("pause");
  if (status.can_seek)
    commands.Append("seek");
  if (status.can_skip_to_next_track)
    commands.Append("queue_next");
  if (status.can_skip_to_previous_track)
    commands.Append("queue_next");
  return commands;
}

Value::List CreateImagesValue(const std::vector<mojom::MediaImagePtr>& images) {
  Value::List image_list;
  for (const mojom::MediaImagePtr& image : images) {
    Value::Dict image_value;
    image_value.Set("url", image->url.spec());
    // CastMediaController should be able to handle images that are missing the
    // width or the height.
    if (image->size) {
      image_value.Set("width", image->size->width());
      image_value.Set("height", image->size->height());
    }
    image_list.Append(std::move(image_value));
  }
  return image_list;
}

Value::Dict CreateMediaStatus(const mojom::MediaStatus& status) {
  Value::Dict status_value;
  status_value.Set("mediaSessionId", Value(kMediaSessionId));
  status_value.Set("media", Value::Dict());
  status_value.SetByDottedPath("media.metadata", Value::Dict());
  status_value.SetByDottedPath("media.metadata.title", Value(status.title));
  status_value.SetByDottedPath("media.metadata.images",
                               CreateImagesValue(status.images));
  status_value.SetByDottedPath("media.duration",
                               Value(status.duration.InSecondsF()));
  status_value.Set("currentTime", Value(status.current_time.InSecondsF()));
  status_value.Set("playerState", GetPlayerStateValue(status));
  status_value.Set("supportedMediaCommands",
                   GetSupportedMediaCommandsValue(status));
  status_value.Set("volume", Value::Dict());
  status_value.SetByDottedPath("volume.level", Value(status.volume));
  status_value.SetByDottedPath("volume.muted", Value(status.is_muted));

  return status_value;
}

mojom::MediaStatusPtr CreateSampleMediaStatus() {
  mojom::MediaStatusPtr status = mojom::MediaStatus::New();
  status->title = kMediaTitle;
  status->can_play_pause = true;
  status->can_mute = true;
  status->can_set_volume = false;
  status->can_seek = false;
  status->can_skip_to_next_track = true;
  status->can_skip_to_previous_track = false;
  status->is_muted = false;
  status->volume = 0.7;
  status->play_state = mojom::MediaStatus::PlayState::BUFFERING;
  status->duration = base::Seconds(30);
  status->current_time = base::Seconds(12);
  return status;
}

std::unique_ptr<CastSession> CreateSampleSession() {
  MediaSinkInternal sink{CreateCastSink("sinkId123", "name"),
                         CastSinkExtraData{}};
  base::Value::Dict receiver_status = base::test::ParseJsonDict(R"({
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
  return CastSession::From(sink, receiver_status);
}

}  // namespace

class CastMediaControllerTest : public testing::Test {
 public:
  CastMediaControllerTest() : activity_(MediaRoute(), "appId123") {}
  ~CastMediaControllerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    mojo::PendingRemote<mojom::MediaStatusObserver> mojo_status_observer;
    status_observer_ = std::make_unique<NiceMock<MockMediaStatusObserver>>(
        mojo_status_observer.InitWithNewPipeAndPassReceiver());
    controller_ = std::make_unique<CastMediaController>(&activity_);
    controller_->AddMediaController(
        mojo_controller_.BindNewPipeAndPassReceiver(),
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
    SetMediaStatus(CreateMediaStatus(status));
  }

  void SetMediaStatus(Value::Dict status_value) {
    Value::Dict status_list;
    status_list.Set("status", Value(Value::List()));
    status_list.FindList("status")->Append(std::move(status_value));

    controller_->SetMediaStatus(status_list);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  NiceMock<MockAppActivity> activity_;
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
        EXPECT_TRUE(cast_message.v2_message_body()
                        .FindByDottedPath("volume.muted")
                        ->GetBool());
        VerifySessionId(cast_message.v2_message_body());
        return 0;
      }));
  mojo_controller_->SetMute(true);
  VerifyAndClearExpectations();

  EXPECT_CALL(activity_, SendSetVolumeRequestToReceiver(_, _))
      .WillOnce(WithArg<0>([](const CastInternalMessage& cast_message) {
        EXPECT_EQ("SET_VOLUME", cast_message.v2_message_type());
        EXPECT_FALSE(cast_message.v2_message_body()
                         .FindByDottedPath("volume.muted")
                         ->GetBool());
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
                                   .FindByDottedPath("volume.level")
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
            cast_message.v2_message_body().Find("currentTime")->GetDouble());
        VerifySessionId(cast_message.v2_message_body());
        return 0;
      });
  mojo_controller_->Seek(base::Seconds(12.34));
}

TEST_F(CastMediaControllerTest, SendNextTrackRequest) {
  SetSessionAndMediaStatus();
  EXPECT_CALL(activity_, SendMediaRequestToReceiver(_))
      .WillOnce([](const CastInternalMessage& cast_message) {
        EXPECT_EQ("QUEUE_UPDATE", cast_message.v2_message_type());
        EXPECT_EQ(1, cast_message.v2_message_body().Find("jump")->GetInt());
        VerifySessionAndMediaSessionIds(cast_message.v2_message_body());
        return 0;
      });
  mojo_controller_->NextTrack();
}

TEST_F(CastMediaControllerTest, SendPreviousTrackRequest) {
  SetSessionAndMediaStatus();
  EXPECT_CALL(activity_, SendMediaRequestToReceiver(_))
      .WillOnce([](const CastInternalMessage& cast_message) {
        EXPECT_EQ("QUEUE_UPDATE", cast_message.v2_message_type());
        EXPECT_EQ(-1, cast_message.v2_message_body().Find("jump")->GetInt());
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
        EXPECT_EQ(expected_status->can_seek, status->can_seek);
        EXPECT_EQ(expected_status->can_skip_to_next_track,
                  status->can_skip_to_next_track);
        EXPECT_EQ(expected_status->can_skip_to_previous_track,
                  status->can_skip_to_previous_track);
        EXPECT_EQ(expected_status->play_state, status->play_state);
        EXPECT_EQ(expected_status->duration, status->duration);
        EXPECT_EQ(expected_status->current_time, status->current_time);
      });
  SetMediaStatus(*expected_status);
  VerifyAndClearExpectations();
}

TEST_F(CastMediaControllerTest, UpdateMediaStatusWithDoubleDurations) {
  mojom::MediaStatusPtr expected_status = CreateSampleMediaStatus();
  expected_status->duration = base::Seconds(30.5);
  expected_status->current_time = base::Seconds(12.9);

  EXPECT_CALL(*status_observer_, OnMediaStatusUpdated(_))
      .WillOnce([&](mojom::MediaStatusPtr status) {
        EXPECT_DOUBLE_EQ(expected_status->duration.InSecondsF(),
                         status->duration.InSecondsF());
        EXPECT_DOUBLE_EQ(expected_status->current_time.InSecondsF(),
                         status->current_time.InSecondsF());
      });
  SetMediaStatus(*expected_status);
  VerifyAndClearExpectations();
}

TEST_F(CastMediaControllerTest, IgnoreInvalidUpdate) {
  Value::Dict invalid_status = CreateMediaStatus(*CreateSampleMediaStatus());
  invalid_status.SetByDottedPath("media.duration", -100);
  invalid_status.SetByDottedPath("currentTime", -100);

  EXPECT_CALL(*status_observer_, OnMediaStatusUpdated(_))
      .WillOnce([&](mojom::MediaStatusPtr status) {
        // Valid fields are copied over.
        EXPECT_EQ(kMediaTitle, status->title);
        // Invalid fields (negative durations) are ignored, and the default
        // value of zero is used.
        EXPECT_EQ(base::Seconds(0), status->duration);
        EXPECT_EQ(base::Seconds(0), status->current_time);
      });
  SetMediaStatus(std::move(invalid_status));
  VerifyAndClearExpectations();
}

TEST_F(CastMediaControllerTest, UpdateMediaImages) {
  mojom::MediaStatusPtr expected_status = CreateSampleMediaStatus();
  expected_status->images.emplace_back(
      std::in_place, GURL("https://example.com/1.png"), gfx::Size(123, 456));
  expected_status->images.emplace_back(
      std::in_place, GURL("https://example.com/2.png"), gfx::Size(789, 0));
  const mojom::MediaImage& image1 = *expected_status->images.at(0);
  const mojom::MediaImage& image2 = *expected_status->images.at(1);

  EXPECT_CALL(*status_observer_, OnMediaStatusUpdated(_))
      .WillOnce([&](const mojom::MediaStatusPtr& status) {
        ASSERT_EQ(2u, status->images.size());
        EXPECT_EQ(image1.url.spec(), status->images.at(0)->url.spec());
        EXPECT_EQ(image1.size->width(), status->images.at(0)->size->width());
        EXPECT_EQ(image1.size->height(), status->images.at(0)->size->height());
        EXPECT_EQ(image2.url.spec(), status->images.at(1)->url.spec());
        EXPECT_EQ(std::nullopt, status->images.at(1)->size);
      });
  SetMediaStatus(*expected_status);
  VerifyAndClearExpectations();
}

TEST_F(CastMediaControllerTest, IgnoreInvalidImage) {
  // Set one valid image and one invalid image.
  mojom::MediaStatusPtr expected_status = CreateSampleMediaStatus();
  expected_status->images.emplace_back(
      std::in_place, GURL("https://example.com/1.png"), gfx::Size(123, 456));
  const mojom::MediaImage& valid_image = *expected_status->images.at(0);
  Value::Dict status_value = CreateMediaStatus(*expected_status);
  status_value.FindListByDottedPath("media.metadata.images")
      ->Append("invalid image");

  EXPECT_CALL(*status_observer_, OnMediaStatusUpdated(_))
      .WillOnce([&](const mojom::MediaStatusPtr& status) {
        ASSERT_EQ(1u, status->images.size());
        EXPECT_EQ(valid_image.url.spec(), status->images.at(0)->url.spec());
      });
  SetMediaStatus(std::move(status_value));
  VerifyAndClearExpectations();
}

TEST_F(CastMediaControllerTest, UpdateVolumeStatus) {
  auto session = CreateSampleSession();
  const float session_volume =
      session->value().FindByDottedPath("receiver.volume.level")->GetDouble();
  const bool session_muted =
      session->value().FindByDottedPath("receiver.volume.muted")->GetBool();
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
