// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/mirroring_activity.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_test_base.h"
#include "chrome/browser/media/router/providers/cast/mock_mirroring_service_host.h"
#include "chrome/browser/media/router/providers/cast/test_util.h"
#include "chrome/browser/media/router/test/media_router_mojo_test.h"
#include "chrome/browser/media/router/test/mock_mojo_media_router.h"
#include "components/media_router/common/mojom/debugger.mojom.h"
#include "components/media_router/common/providers/cast/channel/cast_device_capability.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "media/base/media_switches.h"
#include "media/cast/constants.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

using base::test::IsJson;
using testing::_;
using testing::NiceMock;
using testing::WithArg;
using testing::WithArgs;

namespace media_router {
namespace {

constexpr content::FrameTreeNodeId kFrameTreeNodeId =
    content::FrameTreeNodeId(123);
constexpr int kTabId = 234;
constexpr char kDescription[] = "";
constexpr char kDesktopMediaId[] = "theDesktopMediaId";
constexpr char kDestinationId[] = "theTransportId";
constexpr char kMessageDestinationId[] = "theMessageDestinationId";
constexpr char kMessageSourceId[] = "theMessageSourceId";
constexpr char kNamespace[] = "the_namespace";
constexpr char kPresentationId[] = "thePresentationId";

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

constexpr char kHistogramAudioTransmissionKbps[] =
    "CastStreaming.Sender.Audio.TransmissionRate";
constexpr char kHistogramAudioAverageEncodeTime[] =
    "CastStreaming.Sender.Audio.AverageEncodeTime";
constexpr char kHistogramAudioAverageCaptureLatency[] =
    "CastStreaming.Sender.Audio.AverageCaptureLatency";
constexpr char kHistogramAudioAverageEndToEndLatency[] =
    "CastStreaming.Sender.Audio.AverageEndToEndLatency";
constexpr char kHistogramAudioAverageNetworkLatency[] =
    "CastStreaming.Sender.Audio.AverageNetworkLatency";
constexpr char kHistogramAudioRetransmittedPacketsPercentage[] =
    "CastStreaming.Sender.Audio.RetransmittedPacketsPercentage";
constexpr char kHistogramAudioExceededPlayoutDelayPacketsPercentage[] =
    "CastStreaming.Sender.Audio.ExceededPlayoutDelayPacketsPercentage";
constexpr char kHistogramAudioLateFramesPercentage[] =
    "CastStreaming.Sender.Audio.LateFramesPercentage";
constexpr char kHistogramVideoTransmissionKbps[] =
    "CastStreaming.Sender.Video.TransmissionRate";
constexpr char kHistogramVideoAverageEncodeTime[] =
    "CastStreaming.Sender.Video.AverageEncodeTime";
constexpr char kHistogramVideoAverageCaptureLatency[] =
    "CastStreaming.Sender.Video.AverageCaptureLatency";
constexpr char kHistogramVideoAverageEndToEndLatency[] =
    "CastStreaming.Sender.Video.AverageEndToEndLatency";
constexpr char kHistogramVideoAverageNetworkLatency[] =
    "CastStreaming.Sender.Video.AverageNetworkLatency";
constexpr char kHistogramVideoRetransmittedPacketsPercentage[] =
    "CastStreaming.Sender.Video.RetransmittedPacketsPercentage";
constexpr char kHistogramVideoExceededPlayoutDelayPacketsPercentage[] =
    "CastStreaming.Sender.Video.ExceededPlayoutDelayPacketsPercentage";
constexpr char kHistogramVideoLateFramesPercentage[] =
    "CastStreaming.Sender.Video.LateFramesPercentage";

class MockMirroringServiceHostFactory
    : public mirroring::MirroringServiceHostFactory {
 public:
  MOCK_METHOD(std::unique_ptr<mirroring::MirroringServiceHost>,
              GetForTab,
              (content::FrameTreeNodeId frame_tree_node_id));
  MOCK_METHOD(std::unique_ptr<mirroring::MirroringServiceHost>,
              GetForDesktop,
              (const std::optional<std::string>& media_id));
  MOCK_METHOD(std::unique_ptr<mirroring::MirroringServiceHost>,
              GetForOffscreenTab,
              (const GURL& presentation_url,
               const std::string& presentation_id,
               content::FrameTreeNodeId frame_tree_node_id));
};

class MockCastMessageChannel : public mirroring::mojom::CastMessageChannel {
 public:
  MOCK_METHOD(void, OnMessage, (mirroring::mojom::CastMessagePtr message));
};

class MockMediaRouterDebugger : public mojom::Debugger {
 public:
  MOCK_METHOD(void,
              ShouldFetchMirroringStats,
              (base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(void,
              OnMirroringStats,
              (const base::Value json_stats_cb),
              (override));
  MOCK_METHOD(void,
              BindReceiver,
              (mojo::PendingReceiver<mojom::Debugger> receiver),
              (override));
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

    ON_CALL(media_router_, GetDebugger(_))
        .WillByDefault([this](mojo::PendingReceiver<mojom::Debugger> receiver) {
          auto debugger = std::make_unique<MockMediaRouterDebugger>();
          debugger_object_ = debugger.get();
          mojo::MakeSelfOwnedReceiver(std::move(debugger), std::move(receiver));
        });
  }

  void MakeActivity() { MakeActivity(MediaSource::ForTab(kTabId)); }

  void MakeActivity(
      const MediaSource& source,
      content::FrameTreeNodeId frame_tree_node_id = kFrameTreeNodeId,
      CastDiscoveryType discovery_type = CastDiscoveryType::kMdns,
      bool enable_rtcp_reporting = false) {
    CastSinkExtraData cast_data;
    cast_data.cast_channel_id = kChannelId;
    cast_data.capabilities = {cast_channel::CastDeviceCapability::kAudioOut,
                              cast_channel::CastDeviceCapability::kVideoOut};
    cast_data.discovery_type = discovery_type;
    MediaRoute route(kRouteId, source, kSinkId, kDescription, route_is_local_);
    route.set_presentation_id(kPresentationId);
    activity_ = std::make_unique<MirroringActivity>(
        route, kAppId, &message_handler_, &session_tracker_, frame_tree_node_id,
        cast_data, on_stop_.Get(), on_source_changed_.Get());

    activity_->CreateMojoBindings(&media_router_);

    // This needs to be called before the mojo bindings are created, since we
    // are creating the debugger_object_ at that point.
    ON_CALL(*debugger_object_, ShouldFetchMirroringStats)
        .WillByDefault(
            [enable_rtcp_reporting](base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(enable_rtcp_reporting);
            });
    activity_->CreateMirroringServiceHost(&mirroring_service_host_factory_);
    RunUntilIdle();

    if (route_is_local_) {
      EXPECT_CALL(*mirroring_service_, Start)
          .WillOnce(WithArgs<0, 3>(
              [this](mirroring::mojom::SessionParametersPtr session_params,
                     mojo::PendingReceiver<mirroring::mojom::CastMessageChannel>
                         inbound_channel) {
                ASSERT_FALSE(channel_to_service_);
                auto channel = std::make_unique<MockCastMessageChannel>();
                channel_to_service_ = channel.get();
                mojo::MakeSelfOwnedReceiver(std::move(channel),
                                            std::move(inbound_channel));
                session_params_ = std::move(session_params);
              }));
    }

    activity_->SetOrUpdateSession(*session_, sink_, kHashToken);
    RunUntilIdle();
  }

  const std::string& MessageSourceId() const {
    return message_handler_.source_id();
  }

  bool route_is_local_ = true;
  raw_ptr<MockCastMessageChannel, DanglingUntriaged> channel_to_service_ =
      nullptr;
  raw_ptr<MockMediaRouterDebugger, DanglingUntriaged> debugger_object_ =
      nullptr;
  raw_ptr<MockMirroringServiceHost, DanglingUntriaged> mirroring_service_ =
      nullptr;
  NiceMock<MockMirroringServiceHostFactory> mirroring_service_host_factory_;
  NiceMock<MockMojoMediaRouter> media_router_;
  base::MockCallback<MirroringActivity::OnStopCallback> on_stop_;
  base::MockCallback<OnSourceChangedCallback> on_source_changed_;
  std::unique_ptr<MirroringActivity> activity_;
  mirroring::mojom::SessionParametersPtr session_params_;
};

INSTANTIATE_TEST_SUITE_P(Namespaces,
                         MirroringActivityTest,
                         testing::Values(mirroring::mojom::kWebRtcNamespace,
                                         mirroring::mojom::kRemotingNamespace));

TEST_F(MirroringActivityTest, MirrorDesktop) {
  base::HistogramTester uma_recorder;
  EXPECT_CALL(mirroring_service_host_factory_,
              GetForDesktop(std::optional<std::string>(kDesktopMediaId)));
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
      .WillOnce(WithArg<1>(
          [this](const openscreen::cast::proto::CastMessage& cast_message) {
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
      mirroring::mojom::CastMessage::New(kNamespace, kPayload));
  RunUntilIdle();
}

TEST_F(MirroringActivityTest, SendRemoting) {
  MakeActivity();
  static constexpr char kPayload[] = R"({"type": "RPC"})";
  EXPECT_CALL(message_handler_, SendCastMessage(kChannelId, _))
      .WillOnce(WithArg<1>(
          [](const openscreen::cast::proto::CastMessage& cast_message) {
            EXPECT_EQ(mirroring::mojom::kRemotingNamespace,
                      cast_message.namespace_());
            return cast_channel::Result::kOk;
          }));

  activity_->OnMessage(
      mirroring::mojom::CastMessage::New(kNamespace, kPayload));
  RunUntilIdle();
}

TEST_F(MirroringActivityTest, OnAppMessageWrongNamespace) {
  MakeActivity();
  EXPECT_CALL(*channel_to_service_, OnMessage).Times(0);
  openscreen::cast::proto::CastMessage message;
  message.set_namespace_("wrong_namespace");
  message.set_destination_id(kDestinationId);
  message.set_source_id(MessageSourceId());
  activity_->OnAppMessage(message);
}

TEST_P(MirroringActivityTest, OnAppMessageWrongDestination) {
  MakeActivity();
  EXPECT_CALL(*channel_to_service_, OnMessage).Times(0);
  openscreen::cast::proto::CastMessage message;
  message.set_namespace_(GetParam());
  message.set_destination_id("someOtherDestination");
  message.set_source_id(MessageSourceId());
  activity_->OnAppMessage(message);
}

TEST_P(MirroringActivityTest, OnAppMessageWrongSource) {
  MakeActivity();
  EXPECT_CALL(*channel_to_service_, OnMessage).Times(0);
  openscreen::cast::proto::CastMessage message;
  message.set_namespace_(GetParam());
  message.set_destination_id(kDestinationId);
  message.set_source_id("someRandomStranger");
  activity_->OnAppMessage(message);
}

TEST_P(MirroringActivityTest, OnAppMessageWrongNonlocal) {
  route_is_local_ = false;
  MakeActivity();
  ASSERT_FALSE(channel_to_service_);
  openscreen::cast::proto::CastMessage message;
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

  openscreen::cast::proto::CastMessage message;
  message.set_namespace_(GetParam());
  message.set_destination_id(kDestinationId);
  message.set_source_id(MessageSourceId());
  message.set_protocol_version(
      openscreen::cast::proto::CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_payload_utf8(kPayload);
  activity_->OnAppMessage(message);
}

TEST_F(MirroringActivityTest, OnInternalMessageNonlocal) {
  route_is_local_ = false;
  MakeActivity();
  ASSERT_FALSE(channel_to_service_);
  activity_->OnInternalMessage(cast_channel::InternalMessage(
      cast_channel::CastMessageType::kPing, kMessageSourceId,
      kMessageDestinationId, kNamespace, base::Value::Dict()));
}

TEST_F(MirroringActivityTest, OnInternalMessage) {
  MakeActivity();

  static constexpr char kPayload[] = R"({"foo": "bar"})";

  EXPECT_CALL(*channel_to_service_, OnMessage)
      .WillOnce([](mirroring::mojom::CastMessagePtr message) {
        EXPECT_EQ(kNamespace, message->message_namespace);
        EXPECT_THAT(message->json_format_data, IsJson(kPayload));
      });

  activity_->OnInternalMessage(cast_channel::InternalMessage(
      cast_channel::CastMessageType::kPing, kMessageSourceId,
      kMessageDestinationId, kNamespace, base::test::ParseJsonDict(kPayload)));
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

  std::optional<base::Value> message_json = base::JSONReader::Read(message);
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
  auto* client =
      AddMockClient(activity_.get(), kClientId, content::FrameTreeNodeId(1));
  EXPECT_CALL(*client, SendMessageToClient).WillOnce([=](auto arg) {
    EXPECT_EQ(message_ptr, arg.get());
  });
  activity_->SendMessageToClient(kClientId, std::move(message));
}

TEST_F(MirroringActivityTest, OnSourceChanged) {
  MakeActivity();

  // A random id indicating the new tab source.
  const content::FrameTreeNodeId new_tab_source = content::FrameTreeNodeId(3);

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
      .WillOnce(testing::Return(std::nullopt));
  activity_->OnSourceChanged();
  EXPECT_EQ(activity_->frame_tree_node_id_, new_tab_source);
  testing::Mock::VerifyAndClearExpectations(mirroring_service_);
}

TEST_F(MirroringActivityTest, OnSourceChangedNotifiesMediaStatusObserver) {
  MakeActivity();
  mojo::PendingRemote<mojom::MediaStatusObserver> observer_pending_remote;
  NiceMock<MockMediaStatusObserver> media_status_observer =
      NiceMock<MockMediaStatusObserver>(
          observer_pending_remote.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::MediaController> media_controller;
  activity_->BindMediaController(media_controller.BindNewPipeAndPassReceiver(),
                                 std::move(observer_pending_remote));
  RunUntilIdle();

  // A random value indicating the new tab source.
  const content::FrameTreeNodeId new_tab_source = content::FrameTreeNodeId(3);

  EXPECT_CALL(on_source_changed_, Run(kFrameTreeNodeId, new_tab_source));

  EXPECT_CALL(*mirroring_service_, GetTabSourceId())
      .WillOnce(testing::Return(new_tab_source));

  EXPECT_CALL(media_status_observer, OnMediaStatusUpdated(_))
      .WillOnce([&](mojom::MediaStatusPtr status) {
        EXPECT_EQ(mojom::MediaStatus::PlayState::PLAYING, status->play_state);
      });

  activity_->OnSourceChanged();
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&media_status_observer);
}

TEST_F(MirroringActivityTest, ReportsNotEnabledByDefault) {
  MediaSource source = MediaSource::ForDesktop(kDesktopMediaId, true);
  MakeActivity(source);

  activity_->DidStart();
  EXPECT_FALSE(activity_->should_fetch_stats_on_start_);
}

TEST_F(MirroringActivityTest, EnableRtcpReports) {
  MediaSource source = MediaSource::ForDesktop(kDesktopMediaId, true);
  MakeActivity(source, kFrameTreeNodeId, CastDiscoveryType::kMdns, true);

  activity_->DidStart();
  EXPECT_TRUE(activity_->should_fetch_stats_on_start_);

  ON_CALL(*mirroring_service_, GetMirroringStats(_))
      .WillByDefault([](base::OnceCallback<void(const base::Value)> callback) {
        std::move(callback).Run(base::Value("foo"));
      });

  EXPECT_CALL(*debugger_object_, OnMirroringStats)
      .WillOnce(testing::Invoke([&](const base::Value json_stats_cb) {
        EXPECT_EQ(base::Value("foo"), json_stats_cb);
      }));
  // A call to fetch mirroring stats should have been posted at this point. Fast
  // forward past the delay of this posted task.
  task_environment_.FastForwardBy(media::cast::kRtcpReportInterval);
  RunUntilIdle();
}

TEST_F(MirroringActivityTest, Pause) {
  MakeActivity();
  mojo::PendingRemote<mojom::MediaStatusObserver> observer_pending_remote;
  NiceMock<MockMediaStatusObserver> media_status_observer =
      NiceMock<MockMediaStatusObserver>(
          observer_pending_remote.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::MediaController> media_controller;
  activity_->BindMediaController(media_controller.BindNewPipeAndPassReceiver(),
                                 std::move(observer_pending_remote));
  RunUntilIdle();

  mojom::MediaStatusPtr expected_status = mojom::MediaStatus::New();
  expected_status->play_state = mojom::MediaStatus::PlayState::PAUSED;
  auto cb = [&](base::OnceClosure callback) { std::move(callback).Run(); };
  EXPECT_CALL(*mirroring_service_, Pause(_)).WillOnce(testing::Invoke(cb));
  EXPECT_CALL(media_status_observer, OnMediaStatusUpdated(_))
      .WillOnce([&](mojom::MediaStatusPtr status) {
        EXPECT_EQ(expected_status->play_state, status->play_state);
      });

  activity_->Pause();
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&media_status_observer);
}

TEST_F(MirroringActivityTest, Play) {
  MakeActivity();
  mojo::PendingRemote<mojom::MediaStatusObserver> observer_pending_remote;
  NiceMock<MockMediaStatusObserver> media_status_observer =
      NiceMock<MockMediaStatusObserver>(
          observer_pending_remote.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::MediaController> media_controller;
  activity_->BindMediaController(media_controller.BindNewPipeAndPassReceiver(),
                                 std::move(observer_pending_remote));
  RunUntilIdle();

  mojom::MediaStatusPtr expected_status = mojom::MediaStatus::New();
  expected_status->play_state = mojom::MediaStatus::PlayState::PLAYING;
  auto cb = [&](base::OnceClosure callback) { std::move(callback).Run(); };
  EXPECT_CALL(*mirroring_service_, Resume(_)).WillOnce(testing::Invoke(cb));
  EXPECT_CALL(media_status_observer, OnMediaStatusUpdated(_))
      .WillOnce([&](mojom::MediaStatusPtr status) {
        EXPECT_EQ(expected_status->play_state, status->play_state);
      });

  activity_->Play();
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&media_status_observer);
}

TEST_F(MirroringActivityTest, PauseAndPlay) {
  base::HistogramTester uma_recorder;
  EXPECT_CALL(mirroring_service_host_factory_, GetForTab(kFrameTreeNodeId));
  MediaSource source = MediaSource::ForTab(kTabId);
  MakeActivity(source, kFrameTreeNodeId,
               CastDiscoveryType::kAccessCodeManualEntry);
  auto cb = [&](base::OnceClosure callback) { std::move(callback).Run(); };
  EXPECT_CALL(*mirroring_service_, Pause(_)).WillOnce(testing::Invoke(cb));
  EXPECT_CALL(*mirroring_service_, Resume(_)).WillOnce(testing::Invoke(cb));

  activity_->DidStart();
  activity_->Pause();
  base::RunLoop().RunUntilIdle();
  activity_->Play();
  base::RunLoop().RunUntilIdle();
  activity_.reset();
  base::RunLoop().RunUntilIdle();

  uma_recorder.ExpectTotalCount("AccessCodeCast.Session.FreezeCount", 1);
  uma_recorder.ExpectTotalCount("AccessCodeCast.Session.FreezeDuration", 1);
}

TEST_F(MirroringActivityTest, PauseAndReset) {
  base::HistogramTester uma_recorder;
  EXPECT_CALL(mirroring_service_host_factory_, GetForTab(kFrameTreeNodeId));
  MediaSource source = MediaSource::ForTab(kTabId);
  MakeActivity(source, kFrameTreeNodeId,
               CastDiscoveryType::kAccessCodeManualEntry);
  auto cb = [&](base::OnceClosure callback) { std::move(callback).Run(); };
  EXPECT_CALL(*mirroring_service_, Pause(_)).WillOnce(testing::Invoke(cb));

  activity_->DidStart();
  activity_->Pause();
  base::RunLoop().RunUntilIdle();
  activity_.reset();
  base::RunLoop().RunUntilIdle();

  uma_recorder.ExpectTotalCount("AccessCodeCast.Session.FreezeCount", 1);
  uma_recorder.ExpectTotalCount("AccessCodeCast.Session.FreezeDuration", 1);
}

TEST_F(MirroringActivityTest, OnRemotingStateChanged) {
  MakeActivity();
  mojo::PendingRemote<mojom::MediaStatusObserver> observer_pending_remote;
  NiceMock<MockMediaStatusObserver> media_status_observer =
      NiceMock<MockMediaStatusObserver>(
          observer_pending_remote.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::MediaController> media_controller;
  activity_->BindMediaController(media_controller.BindNewPipeAndPassReceiver(),
                                 std::move(observer_pending_remote));
  RunUntilIdle();

  mojom::MediaStatusPtr expected_status = mojom::MediaStatus::New();
  expected_status->play_state = mojom::MediaStatus::PlayState::PLAYING;
  expected_status->can_play_pause = false;
  EXPECT_CALL(media_status_observer, OnMediaStatusUpdated(_))
      .WillOnce([&](mojom::MediaStatusPtr status) {
        EXPECT_EQ(expected_status->play_state, status->play_state);
        EXPECT_EQ(expected_status->can_play_pause, status->can_play_pause);
      });

  activity_->OnRemotingStateChanged(/*is_remoting*/ true);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&media_status_observer);

  expected_status->can_play_pause = true;
  EXPECT_CALL(media_status_observer, OnMediaStatusUpdated(_))
      .WillOnce([&](mojom::MediaStatusPtr status) {
        EXPECT_EQ(expected_status->play_state, status->play_state);
        EXPECT_EQ(expected_status->can_play_pause, status->can_play_pause);
      });

  activity_->OnRemotingStateChanged(/*is_remoting*/ false);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&media_status_observer);
}

TEST_F(MirroringActivityTest, MultipleMediaControllersNotified) {
  MakeActivity();

  // Set up the first media controller and observer.
  mojo::PendingRemote<mojom::MediaStatusObserver> observer_pending_remote_1;
  NiceMock<MockMediaStatusObserver> media_status_observer_1 =
      NiceMock<MockMediaStatusObserver>(
          observer_pending_remote_1.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::MediaController> media_controller_1;
  activity_->BindMediaController(
      media_controller_1.BindNewPipeAndPassReceiver(),
      std::move(observer_pending_remote_1));

  // Set up the second media controller and observer.
  mojo::PendingRemote<mojom::MediaStatusObserver> observer_pending_remote_2;
  NiceMock<MockMediaStatusObserver> media_status_observer_2 =
      NiceMock<MockMediaStatusObserver>(
          observer_pending_remote_2.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::MediaController> media_controller_2;
  activity_->BindMediaController(
      media_controller_2.BindNewPipeAndPassReceiver(),
      std::move(observer_pending_remote_2));

  // Pause the route, and expect both observers to be notified.
  mojom::MediaStatusPtr expected_status = mojom::MediaStatus::New();
  expected_status->play_state = mojom::MediaStatus::PlayState::PAUSED;
  auto cb = [&](base::OnceClosure callback) { std::move(callback).Run(); };
  EXPECT_CALL(*mirroring_service_, Pause(_)).WillOnce(testing::Invoke(cb));
  EXPECT_CALL(media_status_observer_1, OnMediaStatusUpdated(_))
      .WillOnce([&](mojom::MediaStatusPtr status) {
        EXPECT_EQ(expected_status->play_state, status->play_state);
      });
  EXPECT_CALL(media_status_observer_2, OnMediaStatusUpdated(_))
      .WillOnce([&](mojom::MediaStatusPtr status) {
        EXPECT_EQ(expected_status->play_state, status->play_state);
      });
  activity_->Pause();

  // Ensure the mojom receivers have processed all calls, since we are expecting
  // them to have been called.
  media_status_observer_1.FlushForTesting();
  media_status_observer_2.FlushForTesting();
}

TEST_F(MirroringActivityTest, TargetPlayoutDelaySetInRequest) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kCastMirroringTargetPlayoutDelay,
                                  "300");

  static constexpr char kUrl[] =
      "cast:0F5096E8?streamingCaptureAudio=1&streamingTargetPlayoutDelayMillis="
      "100";
  GURL url(kUrl);
  MediaSource source = MediaSource::ForPresentationUrl(url);
  MakeActivity(source);

  ASSERT_TRUE(session_params_);
  ASSERT_TRUE(session_params_->target_playout_delay.has_value());
  // Target playout delay should be the one set in the request url.
  EXPECT_EQ(session_params_->target_playout_delay.value().InMilliseconds(),
            100);
}

TEST_F(MirroringActivityTest, TargetPlayoutDelayFeatureFlagParam) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kCastMirroringTargetPlayoutDelay,
                                  "300");

  static constexpr char kUrl[] = "cast:0F5096E8?streamingCaptureAudio=1";
  GURL url(kUrl);
  MediaSource source = MediaSource::ForPresentationUrl(url);
  MakeActivity(source);

  ASSERT_TRUE(session_params_);
  ASSERT_TRUE(session_params_->target_playout_delay.has_value());
  EXPECT_EQ(session_params_->target_playout_delay.value().InMilliseconds(),
            300);
}

TEST_F(MirroringActivityTest, CastStreamingSenderUma) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kCastMirroringTargetPlayoutDelay,
                                  "200");

  base::HistogramTester uma_recorder;
  static constexpr char kJsonStats[] = R"({
    "audio": {
      "TRANSMISSION_KBPS": 20.0,
      "AVG_ENCODE_TIME_MS": 13.4,
      "AVG_CAPTURE_LATENCY_MS": 23.7,
      "AVG_E2E_LATENCY_MS": 398.1,
      "AVG_NETWORK_LATENCY_MS": 5.0,
      "NUM_FRAMES_CAPTURED": 500.0,
      "NUM_FRAMES_LATE": 20.0,
      "NUM_PACKETS_SENT": 500.0,
      "NETWORK_LATENCY_MS_HISTO": [
        {"<20": 50.0},
        {"20-39": 150.0},
        {"200-219": 200.0},
        {"300-319": 200.0},
        {">=800": 400.0}
      ]
    },
    "video": {
      "TRANSMISSION_KBPS": 1020.0,
      "AVG_ENCODE_TIME_MS": 9.7,
      "AVG_CAPTURE_LATENCY_MS": 11.3,
      "AVG_E2E_LATENCY_MS": 403.1,
      "AVG_NETWORK_LATENCY_MS": 4.0,
      "NUM_FRAMES_CAPTURED": 600.0,
      "NUM_FRAMES_LATE": 30.0,
      "NUM_PACKETS_SENT": 500.0,
      "NUM_PACKETS_RETRANSMITTED": 50.0,
      "NETWORK_LATENCY_MS_HISTO": [
        {"0-19": 100.0},
        {"200-219": 700.0},
        {"300-319": 200.0}
      ]
    }
    })";
  std::optional<base::Value> stats = base::JSONReader::Read(kJsonStats);
  ASSERT_TRUE(stats.has_value());

  MediaSource source = MediaSource::ForDesktop(kDesktopMediaId, true);
  MakeActivity(source, kFrameTreeNodeId, CastDiscoveryType::kMdns, true);

  activity_->DidStart();

  EXPECT_CALL(*mirroring_service_, GetMirroringStats(_))
      .WillRepeatedly(
          [&stats](base::OnceCallback<void(const base::Value)> callback) {
            std::move(callback).Run(stats->Clone());
          });

  EXPECT_CALL(*debugger_object_, OnMirroringStats)
      .WillOnce(testing::Invoke([&stats](const base::Value json_stats_cb) {
        ASSERT_TRUE(json_stats_cb.is_dict());
        EXPECT_EQ(stats, json_stats_cb.GetDict());
      }));

  // A call to fetch mirroring stats should have been posted at this point. Fast
  // forward past the delay of this posted task.
  task_environment_.FastForwardBy(media::cast::kRtcpReportInterval);
  RunUntilIdle();

  activity_.reset();

  // Check audio UMAs.
  EXPECT_EQ(uma_recorder.GetTotalSum(kHistogramAudioTransmissionKbps), 20);
  EXPECT_EQ(uma_recorder.GetTotalSum(kHistogramAudioAverageEncodeTime), 13);
  EXPECT_EQ(uma_recorder.GetTotalSum(kHistogramAudioAverageCaptureLatency), 23);
  EXPECT_EQ(uma_recorder.GetTotalSum(kHistogramAudioAverageEndToEndLatency),
            398);
  EXPECT_EQ(uma_recorder.GetTotalSum(kHistogramAudioAverageNetworkLatency), 5);
  EXPECT_EQ(uma_recorder.GetTotalSum(kHistogramAudioLateFramesPercentage), 4);

  // Check video UMAs.
  EXPECT_EQ(uma_recorder.GetTotalSum(kHistogramVideoAverageEncodeTime), 9);
  EXPECT_EQ(uma_recorder.GetTotalSum(kHistogramVideoTransmissionKbps), 1020);
  EXPECT_EQ(uma_recorder.GetTotalSum(kHistogramVideoAverageCaptureLatency), 11);
  EXPECT_EQ(uma_recorder.GetTotalSum(kHistogramVideoAverageEndToEndLatency),
            403);
  EXPECT_EQ(uma_recorder.GetTotalSum(kHistogramVideoAverageNetworkLatency), 4);
  EXPECT_EQ(uma_recorder.GetTotalSum(kHistogramVideoLateFramesPercentage), 5);

  // No audio retransmitted packet.
  EXPECT_EQ(
      uma_recorder.GetTotalSum(kHistogramAudioRetransmittedPacketsPercentage),
      0);
  // Video retransmitted packet percentage is 50/500 = 10%
  EXPECT_EQ(
      uma_recorder.GetTotalSum(kHistogramVideoRetransmittedPacketsPercentage),
      10);
  // Audio network latency histo has 300-319 and >=800 buckets where the min
  // latency is above 200ms (target playout delay set by feature flag), sum of
  // packets in these 2 buckets is 200 + 400 = 600, sum of all packets is 1000,
  // so percent is 60%.
  EXPECT_EQ(uma_recorder.GetTotalSum(
                kHistogramAudioExceededPlayoutDelayPacketsPercentage),
            60);
  // Video network latency histo has 300-319 bucket where the min latency is
  // above 200ms (target playout delay set by feature flag), sum of all packets
  // is 1000, so percent is 20%.
  EXPECT_EQ(uma_recorder.GetTotalSum(
                kHistogramVideoExceededPlayoutDelayPacketsPercentage),
            20);
}

}  // namespace media_router
