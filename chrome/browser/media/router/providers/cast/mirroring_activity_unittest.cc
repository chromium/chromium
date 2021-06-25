// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/mirroring_activity.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_test_base.h"
#include "chrome/browser/media/router/providers/cast/test_util.h"
#include "chrome/browser/media/router/test/mock_mojo_media_router.h"
#include "components/cast_channel/cast_test_util.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::test::IsJson;
using testing::_;
using testing::WithArg;

namespace media_router {
namespace {

constexpr int kTabId = 123;
constexpr char kDescription[] = "";
constexpr char kDesktopMediaId[] = "theDesktopMediaId";
constexpr char kPresentationId[] = "thePresentationId";

// Metrics constants.
constexpr char kHistogramSessionLength[] =
    "MediaRouter.CastStreaming.Session.Length";
constexpr char kHistogramSessionLengthDesktop[] =
    "MediaRouter.CastStreaming.Session.Length.Screen";
constexpr char kHistogramSessionLengthFile[] =
    "MediaRouter.CastStreaming.Session.Length.File";
constexpr char kHistogramSessionLengthOffscreenTab[] =
    "MediaRouter.CastStreaming.Session.Length.OffscreenTab";
constexpr char kHistogramSessionLengthTab[] =
    "MediaRouter.CastStreaming.Session.Length.Tab";

class MockMirroringServiceHost : public mirroring::mojom::MirroringServiceHost {
 public:
  MOCK_METHOD4(
      Start,
      void(mirroring::mojom::SessionParametersPtr params,
           mojo::PendingRemote<mirroring::mojom::SessionObserver> observer,
           mojo::PendingRemote<mirroring::mojom::CastMessageChannel>
               outbound_channel,
           mojo::PendingReceiver<mirroring::mojom::CastMessageChannel>
               inbound_channel));
};

class MockCastMessageChannel : public mirroring::mojom::CastMessageChannel {
 public:
  MOCK_METHOD1(Send, void(mirroring::mojom::CastMessagePtr message));
};

}  // namespace

class MirroringActivityTest
    : public CastActivityTestBase,
      public testing::WithParamInterface<const char* /*namespace*/> {
 protected:
  void SetUp() override {
    CastActivityTestBase::SetUp();

    auto make_mirroring_service =
        [this](mojo::PendingReceiver<mirroring::mojom::MirroringServiceHost>
                   receiver) {
          ASSERT_FALSE(mirroring_service_);
          auto mirroring_service = std::make_unique<MockMirroringServiceHost>();
          mirroring_service_ = mirroring_service.get();
          mojo::MakeSelfOwnedReceiver(std::move(mirroring_service),
                                      std::move(receiver));
        };
    ON_CALL(media_router_, GetMirroringServiceHostForDesktop)
        .WillByDefault(WithArg<2>(make_mirroring_service));
    ON_CALL(media_router_, GetMirroringServiceHostForTab)
        .WillByDefault(WithArg<1>(make_mirroring_service));
    ON_CALL(media_router_, GetMirroringServiceHostForOffscreenTab)
        .WillByDefault(WithArg<2>(make_mirroring_service));
  }

  void MakeActivity() { MakeActivity(MediaSource::ForTab(kTabId), kTabId); }

  void MakeActivity(const MediaSource& source, int tab_id = -1) {
    CastSinkExtraData cast_data;
    cast_data.cast_channel_id = kChannelId;
    cast_data.capabilities = cast_channel::AUDIO_OUT | cast_channel::VIDEO_OUT;
    MediaRoute route(kRouteId, source, kSinkId, kDescription, route_is_local_,
                     true);
    route.set_presentation_id(kPresentationId);
    activity_ = std::make_unique<MirroringActivity>(
        route, kAppId, &message_handler_, &session_tracker_, kTabId, cast_data,
        on_stop_.Get());

    activity_->CreateMojoBindings(&media_router_);

    if (route_is_local_) {
      EXPECT_CALL(*mirroring_service_, Start)
          .WillOnce(WithArg<3>(
              [this](mojo::PendingReceiver<mirroring::mojom::CastMessageChannel>
                         inbound_channel) {
                ASSERT_FALSE(channel_to_service_);
                auto channel = std::make_unique<MockCastMessageChannel>();
                channel_to_service_ = channel.get();
                mojo::MakeSelfOwnedReceiver(std::move(channel),
                                            std::move(inbound_channel));
              }));
    }

    activity_->SetOrUpdateSession(*session_, sink_, kHashToken);
    RunUntilIdle();
  }

  bool route_is_local_ = true;
  MockCastMessageChannel* channel_to_service_ = nullptr;
  MockMirroringServiceHost* mirroring_service_ = nullptr;
  MockMojoMediaRouter media_router_;
  base::MockCallback<MirroringActivity::OnStopCallback> on_stop_;
  std::unique_ptr<MirroringActivity> activity_;
};

INSTANTIATE_TEST_CASE_P(Namespaces,
                        MirroringActivityTest,
                        testing::Values(mirroring::mojom::kWebRtcNamespace,
                                        mirroring::mojom::kRemotingNamespace));

TEST_F(MirroringActivityTest, MirrorDesktop) {
  base::HistogramTester uma_recorder;
  EXPECT_CALL(media_router_,
              GetMirroringServiceHostForDesktop(_, kDesktopMediaId, _));
  MediaSource source = MediaSource::ForDesktop(kDesktopMediaId, true);
  ASSERT_TRUE(source.IsDesktopMirroringSource());
  MakeActivity(source);

  activity_->DidStart();
  activity_.reset();

  uma_recorder.ExpectTotalCount(kHistogramSessionLength, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthDesktop, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthFile, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthTab, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthOffscreenTab, 0);
}

TEST_F(MirroringActivityTest, MirrorTab) {
  base::HistogramTester uma_recorder;
  EXPECT_CALL(media_router_, GetMirroringServiceHostForTab(kTabId, _));
  MediaSource source = MediaSource::ForTab(kTabId);
  ASSERT_TRUE(source.IsTabMirroringSource());
  MakeActivity(source, kTabId);

  activity_->DidStart();
  activity_.reset();

  uma_recorder.ExpectTotalCount(kHistogramSessionLength, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthDesktop, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthFile, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthTab, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthOffscreenTab, 0);
}

TEST_F(MirroringActivityTest, CreateMojoBindingsForTabWithCastAppUrl) {
  base::HistogramTester uma_recorder;
  GURL url(kMirroringAppUri);
  EXPECT_CALL(media_router_, GetMirroringServiceHostForTab(kTabId, _));
  MediaSource source = MediaSource::ForPresentationUrl(url);
  ASSERT_TRUE(source.IsCastPresentationUrl());
  MakeActivity(source, kTabId);

  activity_->DidStart();
  activity_.reset();

  uma_recorder.ExpectTotalCount(kHistogramSessionLength, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthDesktop, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthFile, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthTab, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthOffscreenTab, 0);
}

TEST_F(MirroringActivityTest, MirrorOffscreenTab) {
  base::HistogramTester uma_recorder;
  static constexpr char kUrl[] = "http://wikipedia.org";
  GURL url(kUrl);
  EXPECT_CALL(media_router_,
              GetMirroringServiceHostForOffscreenTab(url, kPresentationId, _));
  MediaSource source = MediaSource::ForPresentationUrl(url);
  ASSERT_FALSE(source.IsCastPresentationUrl());
  MakeActivity(source);

  activity_->DidStart();
  activity_.reset();

  uma_recorder.ExpectTotalCount(kHistogramSessionLength, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthDesktop, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthFile, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthTab, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthOffscreenTab, 1);
}

TEST_F(MirroringActivityTest, MirrorFile) {
  base::HistogramTester uma_recorder;
  EXPECT_CALL(media_router_, GetMirroringServiceHostForTab(kTabId, _));
  MediaSource source = MediaSource::ForLocalFile();
  ASSERT_TRUE(source.IsLocalFileSource());
  MakeActivity(source);

  activity_->DidStart();
  activity_.reset();

  uma_recorder.ExpectTotalCount(kHistogramSessionLength, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthDesktop, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthFile, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthTab, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthOffscreenTab, 0);
}

TEST_F(MirroringActivityTest, OnError) {
  MakeActivity();
  EXPECT_CALL(on_stop_, Run());
  activity_->OnError(mirroring::mojom::SessionError::CAST_TRANSPORT_ERROR);
  RunUntilIdle();
}

TEST_F(MirroringActivityTest, DidStop) {
  MakeActivity();
  EXPECT_CALL(on_stop_, Run());
  activity_->DidStop();
  RunUntilIdle();
}

TEST_F(MirroringActivityTest, SendWebRtc) {
  MakeActivity();
  static constexpr char kPayload[] = R"({"foo": "bar"})";
  EXPECT_CALL(message_handler_, SendCastMessage(kChannelId, _))
      .WillOnce(
          WithArg<1>([this](const cast::channel::CastMessage& cast_message) {
            EXPECT_EQ(message_handler_.sender_id(), cast_message.source_id());
            EXPECT_EQ("theTransportId", cast_message.destination_id());
            EXPECT_EQ(mirroring::mojom::kWebRtcNamespace,
                      cast_message.namespace_());
            EXPECT_TRUE(cast_message.has_payload_utf8());
            EXPECT_THAT(cast_message.payload_utf8(), IsJson(kPayload));
            EXPECT_FALSE(cast_message.has_payload_binary());
            return cast_channel::Result::kOk;
          }));

  activity_->Send(
      mirroring::mojom::CastMessage::New("the_namespace", kPayload));
  RunUntilIdle();
}

TEST_F(MirroringActivityTest, SendRemoting) {
  MakeActivity();
  static constexpr char kPayload[] = R"({"type": "RPC"})";
  EXPECT_CALL(message_handler_, SendCastMessage(kChannelId, _))
      .WillOnce(WithArg<1>([](const cast::channel::CastMessage& cast_message) {
        EXPECT_EQ(mirroring::mojom::kRemotingNamespace,
                  cast_message.namespace_());
        return cast_channel::Result::kOk;
      }));

  activity_->Send(
      mirroring::mojom::CastMessage::New("the_namespace", kPayload));
  RunUntilIdle();
}

TEST_F(MirroringActivityTest, OnAppMessageWrongNamespace) {
  MakeActivity();
  EXPECT_CALL(*channel_to_service_, Send).Times(0);
  cast::channel::CastMessage message;
  message.set_namespace_("wrong_namespace");
  activity_->OnAppMessage(message);
}

TEST_P(MirroringActivityTest, OnAppMessageWrongNonlocal) {
  route_is_local_ = false;
  MakeActivity();
  ASSERT_FALSE(channel_to_service_);
  cast::channel::CastMessage message;
  message.set_namespace_(GetParam());
  activity_->OnAppMessage(message);
}

TEST_P(MirroringActivityTest, OnAppMessage) {
  MakeActivity();

  static constexpr char kPayload[] = R"({"foo": "bar"})";

  EXPECT_CALL(*channel_to_service_, Send)
      .WillOnce([](mirroring::mojom::CastMessagePtr message) {
        EXPECT_EQ(GetParam(), message->message_namespace);
        EXPECT_EQ(kPayload, message->json_format_data);
      });

  cast::channel::CastMessage message;
  message.set_namespace_(GetParam());
  message.set_protocol_version(
      cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_payload_utf8(kPayload);
  activity_->OnAppMessage(message);
}

TEST_F(MirroringActivityTest, OnInternalMessageNonlocal) {
  route_is_local_ = false;
  MakeActivity();
  ASSERT_FALSE(channel_to_service_);
  activity_->OnInternalMessage(cast_channel::InternalMessage(
      cast_channel::CastMessageType::kPing, "the_namespace", base::Value()));
}

TEST_F(MirroringActivityTest, OnInternalMessage) {
  MakeActivity();

  static constexpr char kPayload[] = R"({"foo": "bar"})";
  static constexpr char kNamespace[] = "the_namespace";

  EXPECT_CALL(*channel_to_service_, Send)
      .WillOnce([](mirroring::mojom::CastMessagePtr message) {
        EXPECT_EQ(kNamespace, message->message_namespace);
        EXPECT_THAT(message->json_format_data, IsJson(kPayload));
      });

  activity_->OnInternalMessage(cast_channel::InternalMessage(
      cast_channel::CastMessageType::kPing, kNamespace,
      base::test::ParseJson(kPayload)));
}

TEST_F(MirroringActivityTest, GetScrubbedLogMessage) {
  static constexpr char message[] = R"(
    {
      "offer": {
        "supportedStreams": [
          {
            "aesIvMask": "Mask_A",
            "aesKey": "Key_A"
          },
          {
            "aesIvMask": "Mask_B",
            "aesKey": "Key_B"
          }
        ]
      },
      "type": "OFFER"
    })";
  static constexpr char scrubbed_message[] = R"(
    {
      "offer": {
        "supportedStreams": [
          {
            "aesIvMask": "AES_IV_MASK",
            "aesKey": "AES_KEY"
          },
          {
            "aesIvMask": "AES_IV_MASK",
            "aesKey": "AES_KEY"
          }
        ]
      },
      "type": "OFFER"
    })";

  base::Optional<base::Value> message_json = base::JSONReader::Read(message);
  EXPECT_TRUE(message_json);
  EXPECT_THAT(scrubbed_message,
              base::test::IsJson(MirroringActivity::GetScrubbedLogMessage(
                  message_json.value())));
}

}  // namespace media_router
