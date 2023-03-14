// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/mirroring_activity.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_test_base.h"
#include "chrome/browser/media/router/providers/cast/mock_mirroring_service_host.h"
#include "chrome/browser/media/router/providers/cast/test_util.h"
#include "chrome/browser/media/router/test/mock_mojo_media_router.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

using base::test::IsJson;
using testing::_;
using testing::NiceMock;
using testing::WithArg;

namespace media_router {
namespace {

constexpr int kFrameTreeNodeId = 123;
constexpr int kTabId = 234;
constexpr char kDescription[] = "";
constexpr char kDesktopMediaId[] = "theDesktopMediaId";
constexpr char kPresentationId[] = "thePresentationId";
constexpr char kDestinationId[] = "theTransportId";

// Metrics constants.
constexpr char kHistogramSessionLength[] =
    "MediaRouter.CastStreaming.Session.Length";
constexpr char kHistogramSessionLengthAccessCode[] =
    "MediaRouter.CastStreaming.Session.Length.AccessCode";
constexpr char kHistogramSessionLengthDesktop[] =
    "MediaRouter.CastStreaming.Session.Length.Screen";
constexpr char kHistogramSessionLengthOffscreenTab[] =
    "MediaRouter.CastStreaming.Session.Length.OffscreenTab";
constexpr char kHistogramSessionLengthTab[] =
    "MediaRouter.CastStreaming.Session.Length.Tab";

class MockMirroringServiceHostFactory
    : public mirroring::MirroringServiceHostFactory {
 public:
  MOCK_METHOD(std::unique_ptr<mirroring::MirroringServiceHost>,
              GetForTab,
              (int32_t frame_tree_node_id));
  MOCK_METHOD(std::unique_ptr<mirroring::MirroringServiceHost>,
              GetForDesktop,
              (const absl::optional<std::string>& media_id));
  MOCK_METHOD(std::unique_ptr<mirroring::MirroringServiceHost>,
              GetForOffscreenTab,
              (const GURL& presentation_url,
               const std::string& presentation_id,
               int32_t frame_tree_node_id));
};

class MockCastMessageChannel : public mirroring::mojom::CastMessageChannel {
 public:
  MOCK_METHOD(void, OnMessage, (mirroring::mojom::CastMessagePtr message));
};

}  // namespace

class MirroringActivityTest
    : public CastActivityTestBase,
      public testing::WithParamInterface<const char* /*namespace*/> {
 protected:
  void SetUp() override {
    CastActivityTestBase::SetUp();

    auto make_mirroring_service =
        [this]() -> std::unique_ptr<MockMirroringServiceHost> {
      if (!mirroring_service_) {
        auto mirroring_service = std::make_unique<MockMirroringServiceHost>();
        mirroring_service_ = mirroring_service.get();
        return mirroring_service;
      }
      return nullptr;
    };

    ON_CALL(mirroring_service_host_factory_, GetForTab)
        .WillByDefault(make_mirroring_service);
    ON_CALL(mirroring_service_host_factory_, GetForDesktop)
        .WillByDefault(make_mirroring_service);
    ON_CALL(mirroring_service_host_factory_, GetForOffscreenTab)
        .WillByDefault(make_mirroring_service);
  }

  void MakeActivity() { MakeActivity(MediaSource::ForTab(kTabId)); }

  void MakeActivity(
      const MediaSource& source,
      int frame_tree_node_id = kFrameTreeNodeId,
      CastDiscoveryType discovery_type = CastDiscoveryType::kMdns) {
    CastSinkExtraData cast_data;
    cast_data.cast_channel_id = kChannelId;
    cast_data.capabilities = cast_channel::AUDIO_OUT | cast_channel::VIDEO_OUT;
    cast_data.discovery_type = discovery_type;
    MediaRoute route(kRouteId, source, kSinkId, kDescription, route_is_local_);
    route.set_presentation_id(kPresentationId);
    activity_ = std::make_unique<MirroringActivity>(
        route, kAppId, &message_handler_, &session_tracker_, frame_tree_node_id,
        cast_data, on_stop_.Get(), on_source_changed_.Get());

    activity_->CreateMojoBindings(&media_router_);
    activity_->CreateMirroringServiceHost(&mirroring_service_host_factory_);
    RunUntilIdle();

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

  const std::string& MessageSourceId() const {
    return message_handler_.source_id();
  }

  bool route_is_local_ = true;
  raw_ptr<MockCastMessageChannel> channel_to_service_ = nullptr;
  raw_ptr<MockMirroringServiceHost> mirroring_service_ = nullptr;
  NiceMock<MockMirroringServiceHostFactory> mirroring_service_host_factory_;
  NiceMock<MockMojoMediaRouter> media_router_;
  base::MockCallback<MirroringActivity::OnStopCallback> on_stop_;
  base::MockCallback<OnSourceChangedCallback> on_source_changed_;
  std::unique_ptr<MirroringActivity> activity_;
};

INSTANTIATE_TEST_SUITE_P(Namespaces,
                         MirroringActivityTest,
                         testing::Values(mirroring::mojom::kWebRtcNamespace,
                                         mirroring::mojom::kRemotingNamespace));

TEST_F(MirroringActivityTest, MirrorDesktop) {
  base::HistogramTester uma_recorder;
  EXPECT_CALL(mirroring_service_host_factory_,
              GetForDesktop(absl::optional<std::string>(kDesktopMediaId)));
  MediaSource source = MediaSource::ForDesktop(kDesktopMediaId, true);
  ASSERT_TRUE(source.IsDesktopMirroringSource());
  MakeActivity(source);

  activity_->DidStart();
  activity_.reset();

  uma_recorder.ExpectTotalCount(kHistogramSessionLength, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthDesktop, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthTab, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthOffscreenTab, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthAccessCode, 0);
}

TEST_F(MirroringActivityTest, MirrorTab) {
  base::HistogramTester uma_recorder;
  EXPECT_CALL(mirroring_service_host_factory_, GetForTab(kFrameTreeNodeId));
  MediaSource source = MediaSource::ForTab(kTabId);
  ASSERT_TRUE(source.IsTabMirroringSource());
  MakeActivity(source);

  activity_->DidStart();
  activity_.reset();

  uma_recorder.ExpectTotalCount(kHistogramSessionLength, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthDesktop, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthTab, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthOffscreenTab, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthAccessCode, 0);
}

TEST_F(MirroringActivityTest, CreateMojoBindingsForTabWithCastAppUrl) {
  base::HistogramTester uma_recorder;
  EXPECT_CALL(mirroring_service_host_factory_, GetForTab(kFrameTreeNodeId));
  auto site_initiated_mirroring_source =
      CastMediaSource::ForSiteInitiatedMirroring();
  MediaSource source(site_initiated_mirroring_source->source_id());
  ASSERT_TRUE(source.IsCastPresentationUrl());
  MakeActivity(source);

  activity_->DidStart();
  activity_.reset();

  uma_recorder.ExpectTotalCount(kHistogramSessionLength, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthDesktop, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthTab, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthOffscreenTab, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthAccessCode, 0);
}

TEST_F(MirroringActivityTest, MirrorOffscreenTab) {
  base::HistogramTester uma_recorder;
  static constexpr char kUrl[] = "http://wikipedia.org";
  GURL url(kUrl);
  EXPECT_CALL(mirroring_service_host_factory_,
              GetForOffscreenTab(url, kPresentationId, kFrameTreeNodeId));
  MediaSource source = MediaSource::ForPresentationUrl(url);
  ASSERT_FALSE(source.IsCastPresentationUrl());
  MakeActivity(source);

  activity_->DidStart();
  activity_.reset();

  uma_recorder.ExpectTotalCount(kHistogramSessionLength, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthDesktop, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthTab, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthOffscreenTab, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthAccessCode, 0);
}

TEST_F(MirroringActivityTest, MirrorAccessCode) {
  base::HistogramTester uma_recorder;
  EXPECT_CALL(mirroring_service_host_factory_, GetForTab(kFrameTreeNodeId));
  MediaSource source = MediaSource::ForTab(kTabId);
  ASSERT_TRUE(source.IsTabMirroringSource());
  MakeActivity(source, kFrameTreeNodeId,
               CastDiscoveryType::kAccessCodeManualEntry);

  activity_->DidStart();
  activity_.reset();

  uma_recorder.ExpectTotalCount(kHistogramSessionLength, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthDesktop, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthTab, 1);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthOffscreenTab, 0);
  uma_recorder.ExpectTotalCount(kHistogramSessionLengthAccessCode, 1);
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
            EXPECT_EQ(message_handler_.source_id(), cast_message.source_id());
            EXPECT_EQ(kDestinationId, cast_message.destination_id());
            EXPECT_EQ(mirroring::mojom::kWebRtcNamespace,
                      cast_message.namespace_());
            EXPECT_TRUE(cast_message.has_payload_utf8());
            EXPECT_THAT(cast_message.payload_utf8(), IsJson(kPayload));
            EXPECT_FALSE(cast_message.has_payload_binary());
            return cast_channel::Result::kOk;
          }));

  activity_->OnMessage(
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

  activity_->OnMessage(
      mirroring::mojom::CastMessage::New("the_namespace", kPayload));
  RunUntilIdle();
}

TEST_F(MirroringActivityTest, OnAppMessageWrongNamespace) {
  MakeActivity();
  EXPECT_CALL(*channel_to_service_, OnMessage).Times(0);
  cast::channel::CastMessage message;
  message.set_namespace_("wrong_namespace");
  message.set_destination_id(kDestinationId);
  message.set_source_id(MessageSourceId());
  activity_->OnAppMessage(message);
}

TEST_P(MirroringActivityTest, OnAppMessageWrongDestination) {
  MakeActivity();
  EXPECT_CALL(*channel_to_service_, OnMessage).Times(0);
  cast::channel::CastMessage message;
  message.set_namespace_(GetParam());
  message.set_destination_id("someOtherDestination");
  message.set_source_id(MessageSourceId());
  activity_->OnAppMessage(message);
}

TEST_P(MirroringActivityTest, OnAppMessageWrongSource) {
  MakeActivity();
  EXPECT_CALL(*channel_to_service_, OnMessage).Times(0);
  cast::channel::CastMessage message;
  message.set_namespace_(GetParam());
  message.set_destination_id(kDestinationId);
  message.set_source_id("someRandomStranger");
  activity_->OnAppMessage(message);
}

TEST_P(MirroringActivityTest, OnAppMessageWrongNonlocal) {
  route_is_local_ = false;
  MakeActivity();
  ASSERT_FALSE(channel_to_service_);
  cast::channel::CastMessage message;
  message.set_namespace_(GetParam());
  message.set_destination_id(kDestinationId);
  message.set_source_id(MessageSourceId());
  activity_->OnAppMessage(message);
}

TEST_P(MirroringActivityTest, OnAppMessage) {
  MakeActivity();

  static constexpr char kPayload[] = R"({"foo": "bar"})";

  EXPECT_CALL(*channel_to_service_, OnMessage)
      .WillOnce([](mirroring::mojom::CastMessagePtr message) {
        EXPECT_EQ(GetParam(), message->message_namespace);
        EXPECT_EQ(kPayload, message->json_format_data);
      });

  cast::channel::CastMessage message;
  message.set_namespace_(GetParam());
  message.set_destination_id(kDestinationId);
  message.set_source_id(MessageSourceId());
  message.set_protocol_version(
      cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_payload_utf8(kPayload);
  activity_->OnAppMessage(message);
}

TEST_F(MirroringActivityTest, OnInternalMessageNonlocal) {
  route_is_local_ = false;
  MakeActivity();
  ASSERT_FALSE(channel_to_service_);
  activity_->OnInternalMessage(
      cast_channel::InternalMessage(cast_channel::CastMessageType::kPing,
                                    "the_namespace", base::Value::Dict()));
}

TEST_F(MirroringActivityTest, OnInternalMessage) {
  MakeActivity();

  static constexpr char kPayload[] = R"({"foo": "bar"})";
  static constexpr char kNamespace[] = "the_namespace";

  EXPECT_CALL(*channel_to_service_, OnMessage)
      .WillOnce([](mirroring::mojom::CastMessagePtr message) {
        EXPECT_EQ(kNamespace, message->message_namespace);
        EXPECT_THAT(message->json_format_data, IsJson(kPayload));
      });

  activity_->OnInternalMessage(cast_channel::InternalMessage(
      cast_channel::CastMessageType::kPing, kNamespace,
      base::test::ParseJsonDict(kPayload)));
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

  absl::optional<base::Value> message_json = base::JSONReader::Read(message);
  EXPECT_TRUE(message_json);
  EXPECT_TRUE(message_json.value().is_dict());
  EXPECT_THAT(scrubbed_message,
              base::test::IsJson(MirroringActivity::GetScrubbedLogMessage(
                  message_json.value().GetDict())));
}

// Site-initiated mirroring activities must be able to send messages to the
// client, which may be expecting to receive Cast protocol messages.
// See crbug.com/1078481 for context.
TEST_F(MirroringActivityTest, SendMessageToClient) {
  MakeActivity();

  static constexpr char kClientId[] = "theClientId";
  blink::mojom::PresentationConnectionMessagePtr message =
      blink::mojom::PresentationConnectionMessage::NewMessage("\"theMessage\"");
  auto* message_ptr = message.get();
  auto* client = AddMockClient(activity_.get(), kClientId, 1);
  EXPECT_CALL(*client, SendMessageToClient).WillOnce([=](auto arg) {
    EXPECT_EQ(message_ptr, arg.get());
  });
  activity_->SendMessageToClient(kClientId, std::move(message));
}

TEST_F(MirroringActivityTest, OnSourceChanged) {
  MakeActivity();

  // A random int indicating the new tab source.
  const int new_tab_source = 3;

  EXPECT_CALL(on_source_changed_, Run(kFrameTreeNodeId, new_tab_source));

  EXPECT_CALL(*mirroring_service_, GetTabSourceId())
      .WillOnce(testing::Return(new_tab_source));

  EXPECT_EQ(activity_->frame_tree_node_id_, kFrameTreeNodeId);
  activity_->OnSourceChanged();
  EXPECT_EQ(activity_->frame_tree_node_id_, new_tab_source);
  RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mirroring_service_);

  // Nothing should happen as no value was returned for tab source.
  EXPECT_CALL(*mirroring_service_, GetTabSourceId())
      .WillOnce(testing::Return(absl::nullopt));
  activity_->OnSourceChanged();
  EXPECT_EQ(activity_->frame_tree_node_id_, new_tab_source);
  testing::Mock::VerifyAndClearExpectations(mirroring_service_);
}

}  // namespace media_router
