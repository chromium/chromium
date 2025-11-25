// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_receiver_app_ui/boca_receiver_untrusted_page_handler.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/webui/boca_receiver_app_ui/mojom/boca_receiver.mojom-data-view.h"
#include "ash/webui/boca_receiver_app_ui/mojom/boca_receiver.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/invalidations/fcm_handler.h"
#include "chromeos/ash/components/boca/invalidations/invalidation_service_delegate.h"
#include "chromeos/ash/components/boca/invalidations/invalidation_service_impl.h"
#include "chromeos/ash/components/boca/receiver/get_receiver_connection_info_request.h"
#include "chromeos/ash/components/boca/receiver/receiver_handler_delegate.h"
#include "chromeos/ash/components/boca/receiver/register_receiver_request.h"
#include "chromeos/ash/components/boca/receiver/update_kiosk_receiver_state_request.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_audio_stream_consumer.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_remoting_client_manager.h"
#include "chromeos/ash/components/boca/util.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_status_code.h"
#include "remoting/proto/audio.pb.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "url/gurl.h"

namespace ash::boca_receiver {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;

constexpr std::string_view kReceiverId = "AB12";
constexpr std::string_view kConnectionId = "connection-id";
constexpr std::string_view kConnectionCode = "123456";
constexpr std::string_view kStartRequested = "START_REQUESTED";
constexpr std::string_view kInitiatorGaiaId = "initiator-gaia-id";
constexpr std::string_view kPresenterGaiaId = "presenter-gaia-id";
constexpr std::string_view kInitiatorName = "Initiator Name";
constexpr std::string_view kPresenterName = "Presenter Name";
constexpr std::string_view kConnectingPair = R"({"state":"CONNECTING"})";
constexpr std::string_view kConnectedPair = R"({"state":"CONNECTED"})";
constexpr std::string_view kDisconnectedPair = R"({"state":"DISCONNECTED"})";
constexpr std::string_view kErrorPair = R"({"state":"ERROR"})";

constexpr std::string_view kConnectionCodeJson = R"(
          "connectionCode": {
            "connectionCode": "123456"
          },)";
constexpr std::string_view kConnectionInfoTemplate =
    R"({"connectionId": "$1",
        "receiverConnectionState": "$2",
        "connectionDetails": {
          $3
          "initiator": {
            "user": {
              "gaiaId": "$4",
              "email": "initiator@email.com",
              "fullName": "Initiator Name",
              "photoUrl": "http://initiator"
            },
            "deviceInfo": {
              "deviceId": "initiator-device"
            }
          },
          "presenter": {
            "user": {
              "gaiaId": "$5",
              "email": "presenter@email.com",
              "fullName": "Presenter Name",
              "photoUrl": "http://presenter"
            },
            "deviceInfo": {
              "deviceId": "presenter-device"
            }
          }
        }
      })";

class MockUntrustedPage : public mojom::UntrustedPage {
 public:
  MockUntrustedPage() = default;
  ~MockUntrustedPage() override = default;

  mojo::PendingRemote<mojom::UntrustedPage> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void, OnInitReceiverInfo, (mojom::ReceiverInfoPtr), (override));

  MOCK_METHOD(void, OnInitReceiverError, (), (override));

  MOCK_METHOD(void,
              OnConnecting,
              (mojom::UserInfoPtr, mojom::UserInfoPtr),
              (override));

  MOCK_METHOD(void, OnFrameReceived, (const SkBitmap&), (override));

  MOCK_METHOD(void, OnAudioPacket, (mojom::DecodedAudioPacketPtr), (override));

  MOCK_METHOD(void,
              OnConnectionClosed,
              (mojom::ConnectionClosedReason),
              (override));

 private:
  mojo::Receiver<mojom::UntrustedPage> receiver_{this};
};

class MockReceiverHandlerDelegate : public ReceiverHandlerDelegate {
 public:
  MockReceiverHandlerDelegate() = default;
  ~MockReceiverHandlerDelegate() override = default;

  MOCK_METHOD(boca::FCMHandler*, GetFcmHandler, (), (const, override));

  MOCK_METHOD(std::unique_ptr<google_apis::RequestSender>,
              CreateRequestSender,
              (std::string_view, const net::NetworkTrafficAnnotationTag&),
              (const, override));

  MOCK_METHOD(boca::SpotlightRemotingClientManager*,
              GetRemotingClient,
              (),
              (const, override));

  MOCK_METHOD(bool, IsAppEnabled, (std::string_view), (override));
};

class MockSpotlightRemotingClientManager
    : public boca::SpotlightRemotingClientManager {
 public:
  MockSpotlightRemotingClientManager() = default;
  ~MockSpotlightRemotingClientManager() override = default;

  MOCK_METHOD(void,
              StartCrdClient,
              (std::string crd_connection_code,
               base::OnceClosure crd_session_ended_callback,
               boca::SpotlightFrameConsumer::FrameReceivedCallback
                   frame_received_callback,
               boca::SpotlightAudioStreamConsumer::AudioPacketReceivedCallback
                   audio_packet_received_callback,
               boca::SpotlightCrdStateUpdatedCallback status_updated_callback),
              (override));

  MOCK_METHOD(void, StopCrdClient, (base::OnceClosure), (override));

  MOCK_METHOD(std::string, GetDeviceRobotEmail, (), (override));
};

class MockFCMHandler : public boca::FCMHandler {
 public:
  MockFCMHandler() = default;
  ~MockFCMHandler() override = default;

  MOCK_METHOD(void, StartListening, (), (override));
  MOCK_METHOD(void, StopListening, (), (override));
  MOCK_METHOD(void, StopListeningPermanently, (), (override));
  MOCK_METHOD(bool, IsListening, (), (const, override));
  MOCK_METHOD(void, AddListener, (boca::InvalidationsListener*), (override));
  MOCK_METHOD(bool, HasListener, (boca::InvalidationsListener*), (override));
  MOCK_METHOD(void, RemoveListener, (boca::InvalidationsListener*), (override));
  MOCK_METHOD(void,
              AddTokenObserver,
              (boca::FCMRegistrationTokenObserver*),
              (override));
  MOCK_METHOD(void,
              RemoveTokenObserver,
              (boca::FCMRegistrationTokenObserver*),
              (override));
  MOCK_METHOD(const std::optional<std::string>&,
              GetFCMRegistrationToken,
              (),
              (const, override));
};

class BocaReceiverUntrustedPageHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    wifi_service_path_ =
        cros_network_config_helper_.network_state_helper().ConfigureWiFi(
            shill::kStateOnline);
    ON_CALL(handler_delegate_, CreateRequestSender)
        .WillByDefault(
            [this](std::string_view requester_id,
                   const net::NetworkTrafficAnnotationTag& traffic_annotation) {
              return std::make_unique<google_apis::RequestSender>(
                  std::make_unique<google_apis::DummyAuthService>(),
                  url_loader_factory_.GetSafeWeakWrapper(),
                  task_environment_.GetMainThreadTaskRunner(),
                  "test-user-agent", traffic_annotation);
            });
    ON_CALL(handler_delegate_, GetFcmHandler).WillByDefault([this]() {
      return &fcm_handler_;
    });
    ON_CALL(handler_delegate_, IsAppEnabled).WillByDefault(Return(true));
    ON_CALL(fcm_handler_, GetFCMRegistrationToken)
        .WillByDefault([]() -> const std::optional<std::string>& {
          static const std::optional<std::string> kFcmToken = "fcm-token";
          return kFcmToken;
        });
    ON_CALL(fcm_handler_, AddTokenObserver)
        .WillByDefault([this](boca::FCMRegistrationTokenObserver* observer) {
          fcm_token_observer_ = observer;
        });
    ON_CALL(fcm_handler_, StartListening).WillByDefault([this]() {
      ON_CALL(fcm_handler_, IsListening).WillByDefault(Return(true));
      fcm_token_observer_->OnFCMRegistrationTokenChanged();
    });
    ON_CALL(fcm_handler_, StopListening).WillByDefault([this]() {
      ON_CALL(fcm_handler_, IsListening).WillByDefault(Return(false));
    });
    url_loader_factory_.AddResponse(register_url_.spec(),
                                    R"({"receiverId": "AB12"})");
    remoting_client_ =
        std::make_unique<NiceMock<MockSpotlightRemotingClientManager>>();
    ON_CALL(handler_delegate_, GetRemotingClient)
        .WillByDefault(Return(remoting_client_.get()));
  }

  void TearDown() override {
    EXPECT_CALL(fcm_handler_, RemoveListener(handler_.get())).Times(1);
    EXPECT_CALL(fcm_handler_, RemoveTokenObserver(handler_.get())).Times(1);
    fcm_token_observer_ = nullptr;
    handler_.reset();
  }

  std::string CreateConnectionInfo(
      std::string_view connection_id,
      std::string_view connection_state = kStartRequested,
      std::string_view connection_code_json = kConnectionCodeJson,
      std::string_view initiator_gaia_id = kInitiatorGaiaId,
      std::string_view presenter_gaia_id = kPresenterGaiaId) {
    return base::ReplaceStringPlaceholders(
        kConnectionInfoTemplate,
        {std::string(connection_id), std::string(connection_state),
         std::string(connection_code_json), std::string(initiator_gaia_id),
         std::string(presenter_gaia_id)},
        /*offsets=*/nullptr);
  }

  std::string_view GetRequestBody(const GURL& url) {
    url_loader_factory_.WaitForRequest(url);
    const network::TestURLLoaderFactory::PendingRequest* pending_request =
        url_loader_factory_.GetPendingRequest(0);
    const network::ResourceRequestBody* body =
        pending_request->request.request_body.get();
    return (*body->elements())[0]
        .As<network::DataElementBytes>()
        .AsStringPiece();
  }

  void SetDeviceOffline() {
    cros_network_config_helper_.network_state_helper().SetServiceProperty(
        wifi_service_path_, shill::kStateProperty,
        base::Value(shill::kStateNoConnectivity));
  }

  void SetDeviceOnline() {
    cros_network_config_helper_.network_state_helper().SetServiceProperty(
        wifi_service_path_, shill::kStateProperty,
        base::Value(shill::kStateOnline));
  }

  std::string GetConnectionInfoUrlWithConnectionId() {
    return base::StrCat(
        {get_connection_url_.spec(), "?",
         base::ReplaceStringPlaceholders(
             GetReceiverConnectionInfoRequest::kConnectionIdQueryParam,
             {std::string(kConnectionId)}, /*offsets=*/nullptr)});
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network_config::CrosNetworkConfigTestHelper cros_network_config_helper_;
  std::string wifi_service_path_;
  NiceMock<MockFCMHandler> fcm_handler_;
  std::unique_ptr<NiceMock<MockSpotlightRemotingClientManager>>
      remoting_client_;
  std::unique_ptr<BocaReceiverUntrustedPageHandler> handler_;
  raw_ptr<boca::FCMRegistrationTokenObserver> fcm_token_observer_;
  network::TestURLLoaderFactory url_loader_factory_;
  NiceMock<MockReceiverHandlerDelegate> handler_delegate_;
  NiceMock<MockUntrustedPage> page_;
  const GURL register_url_ =
      GURL(boca::GetSchoolToolsUrl()).Resolve(RegisterReceiverRequest::kUrl);
  const GURL get_connection_url_ =
      GURL(boca::GetSchoolToolsUrl())
          .Resolve(base::ReplaceStringPlaceholders(
              GetReceiverConnectionInfoRequest::kRelativeUrlTemplate,
              {std::string(kReceiverId)},
              /*offsets=*/nullptr));
  const GURL update_connection_url_ =
      GURL(boca::GetSchoolToolsUrl())
          .Resolve(base::ReplaceStringPlaceholders(
              UpdateKioskReceiverStateRequest::kRelativeUrlTemplate,
              {std::string(kReceiverId), std::string(kConnectionId)},
              /*offsets=*/nullptr));
};

TEST_F(BocaReceiverUntrustedPageHandlerTest, InitWhenAppDisabled) {
  EXPECT_CALL(handler_delegate_, IsAppEnabled).WillOnce(Return(false));
  base::test::TestFuture<void> signal;
  EXPECT_CALL(page_, OnInitReceiverError).WillOnce([&signal]() {
    signal.GetCallback().Run();
  });
  EXPECT_CALL(fcm_handler_, StartListening).Times(0);
  EXPECT_CALL(handler_delegate_, CreateRequestSender).Times(0);

  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);

  EXPECT_TRUE(signal.Wait());
}

TEST_F(BocaReceiverUntrustedPageHandlerTest, RegisterSuccess) {
  base::test::TestFuture<mojom::ReceiverInfoPtr> on_init_receiver_info_future;
  EXPECT_CALL(page_, OnInitReceiverInfo)
      .WillOnce([&on_init_receiver_info_future](
                    mojom::ReceiverInfoPtr received_info) {
        on_init_receiver_info_future.GetCallback().Run(
            std::move(received_info));
      });
  EXPECT_CALL(fcm_handler_, StartListening).Times(1);
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);

  mojom::ReceiverInfoPtr receiver_info = on_init_receiver_info_future.Take();
  EXPECT_EQ(receiver_info->id, kReceiverId);
  // `GetReceiverConnectionInfoRequest` should be invoked on registration
  // success.
  url_loader_factory_.WaitForRequest(get_connection_url_);
}

TEST_F(BocaReceiverUntrustedPageHandlerTest, RegisterSuccessAlreadyListening) {
  base::test::TestFuture<mojom::ReceiverInfoPtr> on_init_receiver_info_future;
  EXPECT_CALL(page_, OnInitReceiverInfo)
      .WillOnce([&on_init_receiver_info_future](
                    mojom::ReceiverInfoPtr received_info) {
        on_init_receiver_info_future.GetCallback().Run(
            std::move(received_info));
      });
  EXPECT_CALL(fcm_handler_, StartListening).Times(0);
  EXPECT_CALL(fcm_handler_, IsListening).WillOnce(Return(true));
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);

  mojom::ReceiverInfoPtr receiver_info = on_init_receiver_info_future.Take();
  EXPECT_EQ(receiver_info->id, kReceiverId);
  // `GetReceiverConnectionInfoRequest` should be invoked on registration
  // success.
  url_loader_factory_.WaitForRequest(get_connection_url_);
}

TEST_F(BocaReceiverUntrustedPageHandlerTest, RegisterFailure) {
  url_loader_factory_.AddResponse(
      register_url_, network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::HTTP_FORBIDDEN));
  base::test::TestFuture<void> signal;
  EXPECT_CALL(page_, OnInitReceiverError).WillOnce([&signal]() {
    signal.GetCallback().Run();
  });

  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);

  EXPECT_TRUE(signal.Wait());
}

TEST_F(BocaReceiverUntrustedPageHandlerTest, StartRequestedNoCodeThenWithCode) {
  boca::InvalidationsListener* listener = nullptr;
  EXPECT_CALL(fcm_handler_, AddListener)
      .WillOnce([&listener](boca::InvalidationsListener* listener_param) {
        listener = listener_param;
      });
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);
  std::string connection_info_no_code =
      CreateConnectionInfo(kConnectionId, kStartRequested, "");

  EXPECT_CALL(page_, OnConnecting).Times(0);
  EXPECT_CALL(*remoting_client_, StartCrdClient).Times(0);
  url_loader_factory_.WaitForRequest(get_connection_url_);
  url_loader_factory_.SimulateResponseForPendingRequest(
      get_connection_url_.spec(), connection_info_no_code);
  task_environment_.RunUntilIdle();

  url_loader_factory_.AddResponse(get_connection_url_.spec(),
                                  CreateConnectionInfo(kConnectionId));
  base::test::TestFuture<mojom::UserInfoPtr, mojom::UserInfoPtr>
      connecting_future;
  EXPECT_CALL(page_, OnConnecting)
      .WillOnce([&connecting_future](mojom::UserInfoPtr initiator,
                                     mojom::UserInfoPtr presenter) {
        connecting_future.GetCallback().Run(std::move(initiator),
                                            std::move(presenter));
      });
  EXPECT_CALL(*remoting_client_,
              StartCrdClient(std::string(kConnectionCode), _, _, _, _))
      .Times(1);
  listener->OnInvalidationReceived("payload");
  auto [initiator, presenter] = connecting_future.Take();
  ASSERT_FALSE(initiator.is_null());
  EXPECT_EQ(initiator->name, kInitiatorName);
  ASSERT_FALSE(presenter.is_null());
  EXPECT_EQ(presenter->name, kPresenterName);

  // Polling should start since the initiator is not the presenter.
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(
      url_loader_factory_.IsPending(GetConnectionInfoUrlWithConnectionId()));
}

TEST_F(BocaReceiverUntrustedPageHandlerTest,
       StartRequestedInitiatorIsPresenter) {
  url_loader_factory_.AddResponse(
      get_connection_url_.spec(),
      CreateConnectionInfo(kConnectionId, kStartRequested, kConnectionCodeJson,
                           kInitiatorGaiaId, kInitiatorGaiaId));
  base::test::TestFuture<mojom::UserInfoPtr, mojom::UserInfoPtr>
      connecting_future;
  EXPECT_CALL(page_, OnConnecting)
      .WillOnce([&connecting_future](mojom::UserInfoPtr initiator,
                                     mojom::UserInfoPtr presenter) {
        connecting_future.GetCallback().Run(std::move(initiator),
                                            std::move(presenter));
      });
  EXPECT_CALL(*remoting_client_,
              StartCrdClient(std::string(kConnectionCode), _, _, _, _))
      .Times(1);
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);

  auto [initiator, presenter] = connecting_future.Take();
  ASSERT_FALSE(initiator.is_null());
  EXPECT_EQ(initiator->name, kInitiatorName);
  EXPECT_TRUE(presenter.is_null());

  // Polling should not start since the initiator is the presenter.
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_FALSE(
      url_loader_factory_.IsPending(GetConnectionInfoUrlWithConnectionId()));
}

TEST_F(BocaReceiverUntrustedPageHandlerTest, FrameReceived) {
  url_loader_factory_.AddResponse(get_connection_url_.spec(),
                                  CreateConnectionInfo(kConnectionId));
  base::RepeatingCallback<void(SkBitmap, std::unique_ptr<webrtc::DesktopFrame>)>
      frame_received_cb;
  EXPECT_CALL(*remoting_client_,
              StartCrdClient(std::string(kConnectionCode), _, _, _, _))
      .WillOnce([&frame_received_cb](auto, auto, auto frame_received_cb_param,
                                     auto, auto) {
        frame_received_cb = std::move(frame_received_cb_param);
      });
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);

  // Verify the first state update to CONNECTING.
  EXPECT_EQ(GetRequestBody(update_connection_url_), kConnectingPair);
  url_loader_factory_.SimulateResponseForPendingRequest(
      update_connection_url_.spec(), kConnectingPair);

  ASSERT_FALSE(frame_received_cb.is_null());
  // First frame received.
  base::test::TestFuture<const SkBitmap&> frame_future;
  EXPECT_CALL(page_, OnFrameReceived(_))
      .WillOnce([&frame_future](const SkBitmap& bitmap) {
        frame_future.GetCallback().Run(bitmap);
      });
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.eraseColor(SK_ColorRED);
  frame_received_cb.Run(bitmap, /*desktop_frame=*/nullptr);
  // The first frame should trigger an update to CONNECTED state.
  EXPECT_EQ(GetRequestBody(update_connection_url_), kConnectedPair);
  url_loader_factory_.SimulateResponseForPendingRequest(
      update_connection_url_.spec(), kConnectedPair);

  const SkBitmap& received_bitmap = frame_future.Get();
  EXPECT_EQ(received_bitmap.getColor(0, 0), SK_ColorRED);
  EXPECT_EQ(received_bitmap.width(), 10);
  EXPECT_EQ(received_bitmap.height(), 10);

  // Second frame received. No more state updates should be sent.
  EXPECT_CALL(page_, OnFrameReceived(_)).Times(1);
  SkBitmap bitmap2;
  bitmap2.allocN32Pixels(20, 20);
  bitmap2.eraseColor(SK_ColorBLUE);
  frame_received_cb.Run(bitmap2, /*desktop_frame=*/nullptr);

  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
}

TEST_F(BocaReceiverUntrustedPageHandlerTest, AudioPacketReceived) {
  url_loader_factory_.AddResponse(get_connection_url_.spec(),
                                  CreateConnectionInfo(kConnectionId));
  base::RepeatingCallback<void(std::unique_ptr<remoting::AudioPacket> packet)>
      audio_packet_received_cb;
  EXPECT_CALL(*remoting_client_,
              StartCrdClient(std::string(kConnectionCode), _, _, _, _))
      .WillOnce([&audio_packet_received_cb](auto, auto, auto,
                                            auto audio_packet_received_cb_param,
                                            auto) {
        audio_packet_received_cb = std::move(audio_packet_received_cb_param);
      });
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);

  url_loader_factory_.SimulateResponseForPendingRequest(
      update_connection_url_.spec(), kConnectingPair);

  ASSERT_FALSE(audio_packet_received_cb.is_null());
  // First audio packet received.
  base::test::TestFuture<mojom::DecodedAudioPacketPtr> audio_packet_future;
  EXPECT_CALL(page_, OnAudioPacket(_))
      .Times(1)
      .WillOnce(
          [&audio_packet_future](mojom::DecodedAudioPacketPtr decoded_packet) {
            audio_packet_future.GetCallback().Run(std::move(decoded_packet));
          });

  std::unique_ptr<remoting::AudioPacket> fake_packet =
      std::make_unique<remoting::AudioPacket>();
  fake_packet->set_encoding(remoting::AudioPacket::ENCODING_RAW);
  fake_packet->set_bytes_per_sample(remoting::AudioPacket::BYTES_PER_SAMPLE_2);
  fake_packet->set_sampling_rate(remoting::AudioPacket::SAMPLING_RATE_48000);
  fake_packet->set_channels(remoting::AudioPacket::CHANNELS_STEREO);
  const std::array<int16_t, 4> test_data = {1, 2, 3, 4};
  fake_packet->add_data(reinterpret_cast<const char*>(test_data.data()),
                        sizeof(test_data));
  audio_packet_received_cb.Run(std::move(fake_packet));

  mojom::DecodedAudioPacketPtr received_packet = audio_packet_future.Take();
  EXPECT_EQ(received_packet->sample_rate,
            remoting::AudioPacket::SAMPLING_RATE_48000);
  EXPECT_EQ(received_packet->channels, remoting::AudioPacket::CHANNELS_STEREO);
  ASSERT_EQ(received_packet->data.size(), std::size(test_data));
  for (size_t i = 0; i < test_data.size(); ++i) {
    EXPECT_EQ(received_packet->data[i], test_data[i]);
  }
}

TEST_F(BocaReceiverUntrustedPageHandlerTest, InvalidAudioPacketNotSent) {
  url_loader_factory_.AddResponse(get_connection_url_.spec(),
                                  CreateConnectionInfo(kConnectionId));
  base::RepeatingCallback<void(std::unique_ptr<remoting::AudioPacket> packet)>
      audio_packet_received_cb;
  EXPECT_CALL(*remoting_client_,
              StartCrdClient(std::string(kConnectionCode), _, _, _, _))
      .WillOnce([&audio_packet_received_cb](auto, auto, auto,
                                            auto audio_packet_received_cb_param,
                                            auto) {
        audio_packet_received_cb = std::move(audio_packet_received_cb_param);
      });
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);

  url_loader_factory_.SimulateResponseForPendingRequest(
      update_connection_url_.spec(), kConnectingPair);

  ASSERT_FALSE(audio_packet_received_cb.is_null());
  // Expect OnAudioPacket to never be called due to the invalid packet.
  EXPECT_CALL(page_, OnAudioPacket(_)).Times(0);

  std::unique_ptr<remoting::AudioPacket> fake_invalid_packet =
      std::make_unique<remoting::AudioPacket>();
  // Invalid encoding.
  fake_invalid_packet->set_encoding(remoting::AudioPacket::ENCODING_OPUS);
  fake_invalid_packet->set_bytes_per_sample(
      remoting::AudioPacket::BYTES_PER_SAMPLE_2);
  fake_invalid_packet->set_sampling_rate(
      remoting::AudioPacket::SAMPLING_RATE_48000);
  fake_invalid_packet->set_channels(remoting::AudioPacket::CHANNELS_STEREO);
  const std::array<int16_t, 4> test_data = {1, 2, 3, 4};
  fake_invalid_packet->add_data(reinterpret_cast<const char*>(test_data.data()),
                                sizeof(test_data));
  audio_packet_received_cb.Run(std::move(fake_invalid_packet));
}

TEST_F(BocaReceiverUntrustedPageHandlerTest, CrdSessionEnded) {
  url_loader_factory_.AddResponse(get_connection_url_.spec(),
                                  CreateConnectionInfo(kConnectionId));
  base::OnceClosure session_ended_cb;
  EXPECT_CALL(*remoting_client_,
              StartCrdClient(std::string(kConnectionCode), _, _, _, _))
      .WillOnce([&session_ended_cb](auto,
                                    base::OnceClosure session_ended_cb_param,
                                    auto, auto, auto) {
        session_ended_cb = std::move(session_ended_cb_param);
      });
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);

  // Verify the first state update to CONNECTING.
  EXPECT_EQ(GetRequestBody(update_connection_url_), kConnectingPair);
  url_loader_factory_.SimulateResponseForPendingRequest(
      update_connection_url_.spec(), kConnectingPair);

  ASSERT_FALSE(session_ended_cb.is_null());
  base::test::TestFuture<mojom::ConnectionClosedReason>
      connection_closed_future;
  EXPECT_CALL(page_, OnConnectionClosed)
      .WillOnce(
          [&connection_closed_future](mojom::ConnectionClosedReason reason) {
            connection_closed_future.GetCallback().Run(reason);
          });
  std::move(session_ended_cb).Run();

  EXPECT_EQ(connection_closed_future.Get(),
            mojom::ConnectionClosedReason::kPresenterConnectionLost);
  EXPECT_EQ(GetRequestBody(update_connection_url_), kDisconnectedPair);
}

TEST_F(BocaReceiverUntrustedPageHandlerTest,
       StartRequestedWithDifferentConnectionId) {
  url_loader_factory_.AddResponse(get_connection_url_.spec(),
                                  CreateConnectionInfo(kConnectionId));
  EXPECT_CALL(*remoting_client_, StartCrdClient).Times(1);
  boca::InvalidationsListener* listener = nullptr;
  EXPECT_CALL(fcm_handler_, AddListener)
      .WillOnce([&listener](boca::InvalidationsListener* listener_param) {
        listener = listener_param;
      });
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);
  EXPECT_EQ(GetRequestBody(update_connection_url_), kConnectingPair);
  url_loader_factory_.SimulateResponseForPendingRequest(
      update_connection_url_.spec(), kConnectingPair);

  // New connection request with different ID.
  constexpr std::string_view kNewConnectionId = "new-connection-id";
  const GURL kUpdateNewConnectionUrl =
      GURL(boca::GetSchoolToolsUrl())
          .Resolve(base::ReplaceStringPlaceholders(
              UpdateKioskReceiverStateRequest::kRelativeUrlTemplate,
              {std::string(kReceiverId), std::string(kNewConnectionId)},
              /*offsets=*/nullptr));
  url_loader_factory_.AddResponse(get_connection_url_.spec(),
                                  CreateConnectionInfo(kNewConnectionId));

  base::test::TestFuture<mojom::ConnectionClosedReason>
      connection_closed_future;
  EXPECT_CALL(page_, OnConnectionClosed)
      .WillOnce(
          [&connection_closed_future](mojom::ConnectionClosedReason reason) {
            connection_closed_future.GetCallback().Run(reason);
          });
  EXPECT_CALL(*remoting_client_, StopCrdClient).Times(1);
  EXPECT_CALL(*remoting_client_, StartCrdClient).Times(1);

  listener->OnInvalidationReceived("payload");

  EXPECT_EQ(connection_closed_future.Get(),
            mojom::ConnectionClosedReason::kTakeOver);
  EXPECT_EQ(GetRequestBody(update_connection_url_), kDisconnectedPair);
  url_loader_factory_.SimulateResponseForPendingRequest(
      update_connection_url_.spec(), kDisconnectedPair);
  EXPECT_EQ(GetRequestBody(kUpdateNewConnectionUrl), kConnectingPair);
}

TEST_F(BocaReceiverUntrustedPageHandlerTest, StopRequestedBeforeConnecting) {
  // Establish a connection first.
  url_loader_factory_.AddResponse(
      get_connection_url_.spec(),
      CreateConnectionInfo(kConnectionId, "START_REQUESTED", ""));
  boca::InvalidationsListener* listener = nullptr;
  EXPECT_CALL(fcm_handler_, AddListener)
      .WillOnce([&listener](boca::InvalidationsListener* listener_param) {
        listener = listener_param;
      });
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);
  // Now simulate a STOP_REQUESTED invalidation for the same connection.
  url_loader_factory_.AddResponse(
      get_connection_url_.spec(),
      CreateConnectionInfo(kConnectionId, "STOP_REQUESTED"));

  EXPECT_CALL(page_, OnConnectionClosed).Times(0);
  EXPECT_CALL(*remoting_client_, StopCrdClient).Times(0);

  listener->OnInvalidationReceived("payload");
  EXPECT_EQ(GetRequestBody(update_connection_url_), kDisconnectedPair);
}

TEST_F(BocaReceiverUntrustedPageHandlerTest, StopRequestedAfterConnecting) {
  // Establish a connection first.
  url_loader_factory_.AddResponse(get_connection_url_.spec(),
                                  CreateConnectionInfo(kConnectionId));
  EXPECT_CALL(*remoting_client_, StartCrdClient).Times(1);
  boca::InvalidationsListener* listener = nullptr;
  EXPECT_CALL(fcm_handler_, AddListener)
      .WillOnce([&listener](boca::InvalidationsListener* listener_param) {
        listener = listener_param;
      });
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);
  // Wait for CONNECTING update.
  url_loader_factory_.WaitForRequest(update_connection_url_);
  url_loader_factory_.SimulateResponseForPendingRequest(
      update_connection_url_.spec(), kConnectingPair);

  // Now simulate a STOP_REQUESTED invalidation for the same connection.
  url_loader_factory_.AddResponse(
      get_connection_url_.spec(),
      CreateConnectionInfo(kConnectionId, "STOP_REQUESTED"));

  base::test::TestFuture<mojom::ConnectionClosedReason>
      connection_closed_future;
  EXPECT_CALL(page_, OnConnectionClosed)
      .WillOnce(
          [&connection_closed_future](mojom::ConnectionClosedReason reason) {
            connection_closed_future.GetCallback().Run(reason);
          });
  EXPECT_CALL(*remoting_client_, StopCrdClient).Times(1);

  listener->OnInvalidationReceived("payload");

  EXPECT_EQ(connection_closed_future.Get(),
            mojom::ConnectionClosedReason::kInitiatorClosed);
  EXPECT_EQ(GetRequestBody(update_connection_url_), kDisconnectedPair);

  // No more polling after stop.
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_FALSE(
      url_loader_factory_.IsPending(GetConnectionInfoUrlWithConnectionId()));
}

TEST_F(BocaReceiverUntrustedPageHandlerTest, StopRequestedFetchedByPolling) {
  base::test::TestFuture<mojom::ConnectionClosedReason>
      connection_closed_future;
  // Establish a connection first.
  url_loader_factory_.AddResponse(get_connection_url_.spec(),
                                  CreateConnectionInfo(kConnectionId));
  EXPECT_CALL(*remoting_client_, StartCrdClient).Times(1);
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);
  // Wait for CONNECTING update.
  url_loader_factory_.WaitForRequest(update_connection_url_);
  url_loader_factory_.SimulateResponseForPendingRequest(
      update_connection_url_.spec(), kConnectingPair);

  // Now simulate a STOP_REQUESTED response for polling.
  url_loader_factory_.AddResponse(
      GetConnectionInfoUrlWithConnectionId(),
      CreateConnectionInfo(kConnectionId, "STOP_REQUESTED"));
  EXPECT_CALL(page_, OnConnectionClosed)
      .WillOnce(
          [&connection_closed_future](mojom::ConnectionClosedReason reason) {
            connection_closed_future.GetCallback().Run(reason);
          });
  EXPECT_CALL(*remoting_client_, StopCrdClient).Times(1);

  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_EQ(connection_closed_future.Get(),
            mojom::ConnectionClosedReason::kInitiatorClosed);
  EXPECT_EQ(GetRequestBody(update_connection_url_), kDisconnectedPair);

  // No more polling after stop.
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_FALSE(
      url_loader_factory_.IsPending(GetConnectionInfoUrlWithConnectionId()));
}

TEST_F(BocaReceiverUntrustedPageHandlerTest, StopByPollingFailure) {
  base::test::TestFuture<mojom::ConnectionClosedReason>
      connection_closed_future;
  // Establish a connection first.
  url_loader_factory_.AddResponse(get_connection_url_.spec(),
                                  CreateConnectionInfo(kConnectionId));
  EXPECT_CALL(*remoting_client_, StartCrdClient).Times(1);
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);
  // Wait for CONNECTING update.
  url_loader_factory_.WaitForRequest(update_connection_url_);
  url_loader_factory_.SimulateResponseForPendingRequest(
      update_connection_url_.spec(), kConnectingPair);

  // Simulate polling failure.
  url_loader_factory_.AddResponse(GetConnectionInfoUrlWithConnectionId(),
                                  /*content=*/"",
                                  net::HTTP_SERVICE_UNAVAILABLE);
  EXPECT_CALL(page_, OnConnectionClosed)
      .WillOnce(
          [&connection_closed_future](mojom::ConnectionClosedReason reason) {
            connection_closed_future.GetCallback().Run(reason);
          });
  EXPECT_CALL(*remoting_client_, StopCrdClient).Times(1);

  task_environment_.FastForwardBy(base::Seconds(30));

  EXPECT_EQ(connection_closed_future.Get(),
            mojom::ConnectionClosedReason::kError);
  EXPECT_EQ(GetRequestBody(update_connection_url_), kErrorPair);

  // No more polling after stop.
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_FALSE(
      url_loader_factory_.IsPending(GetConnectionInfoUrlWithConnectionId()));
}

TEST_F(BocaReceiverUntrustedPageHandlerTest, StopRequestedDifferentConnection) {
  // Establish a connection first.
  url_loader_factory_.AddResponse(get_connection_url_.spec(),
                                  CreateConnectionInfo(kConnectionId));
  EXPECT_CALL(*remoting_client_, StartCrdClient).Times(1);
  boca::InvalidationsListener* listener = nullptr;
  EXPECT_CALL(fcm_handler_, AddListener)
      .WillOnce([&listener](boca::InvalidationsListener* listener_param) {
        listener = listener_param;
      });
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);
  // Wait for CONNECTING update.
  url_loader_factory_.WaitForRequest(update_connection_url_);
  url_loader_factory_.SimulateResponseForPendingRequest(
      update_connection_url_.spec(), kConnectingPair);

  // Now simulate a STOP_REQUESTED invalidation for a different connection.
  constexpr std::string_view kOldConnectionId = "old-connection-id";
  const GURL kUpdateOldConnectionUrl =
      GURL(boca::GetSchoolToolsUrl())
          .Resolve(base::ReplaceStringPlaceholders(
              UpdateKioskReceiverStateRequest::kRelativeUrlTemplate,
              {std::string(kReceiverId), std::string(kOldConnectionId)},
              /*offsets=*/nullptr));
  url_loader_factory_.AddResponse(
      get_connection_url_.spec(),
      CreateConnectionInfo(kOldConnectionId, "STOP_REQUESTED"));

  EXPECT_CALL(page_, OnConnectionClosed).Times(0);
  EXPECT_CALL(*remoting_client_, StopCrdClient).Times(0);

  listener->OnInvalidationReceived("payload");

  EXPECT_EQ(GetRequestBody(kUpdateOldConnectionUrl), kDisconnectedPair);
}

TEST_F(BocaReceiverUntrustedPageHandlerTest, InitNetowrkOffline) {
  base::test::TestFuture<void> offline_error_signal;
  base::test::TestFuture<void> on_init_receiver_info_signal;
  SetDeviceOffline();
  EXPECT_CALL(page_, OnInitReceiverError).WillOnce([&offline_error_signal]() {
    offline_error_signal.GetCallback().Run();
  });
  EXPECT_CALL(fcm_handler_, StartListening).Times(0);

  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);

  EXPECT_TRUE(offline_error_signal.Wait());

  EXPECT_CALL(fcm_handler_, StartListening).Times(1);
  EXPECT_CALL(page_, OnInitReceiverInfo)
      .WillOnce([&on_init_receiver_info_signal](mojom::ReceiverInfoPtr) {
        on_init_receiver_info_signal.GetCallback().Run();
      });
  SetDeviceOnline();

  EXPECT_TRUE(on_init_receiver_info_signal.Wait());
}

TEST_F(BocaReceiverUntrustedPageHandlerTest,
       BackOnlineBeforeFetchCompleteWithSuccess) {
  base::test::TestFuture<void> offline_error_signal;
  base::test::TestFuture<void> on_init_receiver_info_signal;
  EXPECT_CALL(page_, OnInitReceiverError).WillOnce([&offline_error_signal]() {
    offline_error_signal.GetCallback().Run();
  });
  EXPECT_CALL(fcm_handler_, StartListening).WillOnce(Return());
  EXPECT_CALL(fcm_handler_, StopListening).Times(0);

  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);
  SetDeviceOffline();
  EXPECT_TRUE(offline_error_signal.Wait());

  EXPECT_CALL(page_, OnInitReceiverInfo)
      .WillOnce([&on_init_receiver_info_signal](mojom::ReceiverInfoPtr) {
        on_init_receiver_info_signal.GetCallback().Run();
      });
  SetDeviceOnline();
  // Simulate FCM token fetch success.
  fcm_token_observer_->OnFCMRegistrationTokenChanged();

  EXPECT_TRUE(on_init_receiver_info_signal.Wait());
}

TEST_F(BocaReceiverUntrustedPageHandlerTest,
       BackOnlineBeforeFetchCompleteWithError) {
  base::test::TestFuture<void> offline_error_signal;
  base::test::TestFuture<void> on_init_receiver_info_signal;
  EXPECT_CALL(page_, OnInitReceiverError).WillOnce([&offline_error_signal]() {
    offline_error_signal.GetCallback().Run();
  });
  EXPECT_CALL(fcm_handler_, StartListening).WillOnce(Return());

  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);
  SetDeviceOffline();
  EXPECT_TRUE(offline_error_signal.Wait());

  EXPECT_CALL(page_, OnInitReceiverInfo)
      .WillOnce([&on_init_receiver_info_signal](mojom::ReceiverInfoPtr) {
        on_init_receiver_info_signal.GetCallback().Run();
      });
  SetDeviceOnline();
  // Simulate FCM token fetch failure. Fetch should be retried since it started
  // before device goes offline.
  EXPECT_CALL(fcm_handler_, StopListening).Times(1);
  EXPECT_CALL(fcm_handler_, StartListening).WillOnce([this]() {
    // Simulate FCM token fetch retry success.
    fcm_token_observer_->OnFCMRegistrationTokenChanged();
  });
  fcm_token_observer_->OnFCMTokenFetchFailed();

  EXPECT_TRUE(on_init_receiver_info_signal.Wait());
}

TEST_F(BocaReceiverUntrustedPageHandlerTest,
       BackOnlineBeforeFetchCompleteWithErrorRetryOnlyOnce) {
  base::test::TestFuture<void> offline_error_signal;
  base::test::TestFuture<void> retry_error_signal;
  EXPECT_CALL(page_, OnInitReceiverError).WillOnce([&offline_error_signal]() {
    offline_error_signal.GetCallback().Run();
  });
  EXPECT_CALL(page_, OnInitReceiverInfo).Times(0);
  EXPECT_CALL(fcm_handler_, StartListening).WillOnce(Return());

  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);
  SetDeviceOffline();
  EXPECT_TRUE(offline_error_signal.Wait());

  SetDeviceOnline();
  // Simulate FCM token fetch failure. Fetch should be retried since it started
  // before device goes offline.
  EXPECT_CALL(page_, OnInitReceiverError).WillOnce([&retry_error_signal]() {
    retry_error_signal.GetCallback().Run();
  });
  EXPECT_CALL(fcm_handler_, StopListening).Times(1);
  EXPECT_CALL(fcm_handler_, StartListening).WillOnce([this]() {
    // Simulate FCM token fetch retry failure.
    fcm_token_observer_->OnFCMTokenFetchFailed();
  });
  fcm_token_observer_->OnFCMTokenFetchFailed();

  EXPECT_TRUE(retry_error_signal.Wait());
}

TEST_F(BocaReceiverUntrustedPageHandlerTest, GoesOfflineAfterRegisterSuccess) {
  base::test::TestFuture<void> on_init_receiver_info_first_signal;
  base::test::TestFuture<void> offline_error_signal;
  base::test::TestFuture<void> on_init_receiver_info_second_signal;
  EXPECT_CALL(page_, OnInitReceiverInfo)
      .WillOnce([&on_init_receiver_info_first_signal](mojom::ReceiverInfoPtr) {
        on_init_receiver_info_first_signal.GetCallback().Run();
      });
  EXPECT_CALL(fcm_handler_, StartListening).Times(1);
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);

  EXPECT_TRUE(on_init_receiver_info_first_signal.Wait());

  url_loader_factory_.WaitForRequest(get_connection_url_);
  url_loader_factory_.SimulateResponseForPendingRequest(
      get_connection_url_.spec(),
      CreateConnectionInfo(kConnectionId, "DISCONNECTED"));

  EXPECT_CALL(page_, OnInitReceiverError).WillOnce([&offline_error_signal]() {
    offline_error_signal.GetCallback().Run();
  });
  SetDeviceOffline();
  EXPECT_TRUE(offline_error_signal.Wait());

  EXPECT_CALL(page_, OnInitReceiverInfo)
      .WillOnce([&on_init_receiver_info_second_signal](mojom::ReceiverInfoPtr) {
        on_init_receiver_info_second_signal.GetCallback().Run();
      });
  SetDeviceOnline();
  EXPECT_TRUE(on_init_receiver_info_second_signal.Wait());
  // If receiver is already initialized, it should issue a get connection
  // request when it comes back online.
  url_loader_factory_.WaitForRequest(get_connection_url_);
}

TEST_F(BocaReceiverUntrustedPageHandlerTest,
       GoesOfflineAfterAfterFcmTokenFetchBeforeRegisterSuccess) {
  base::test::TestFuture<mojom::ReceiverInfoPtr> on_init_receiver_info_future;
  base::test::TestFuture<void> register_error_signal;
  base::test::TestFuture<void> offline_error_signal;

  url_loader_factory_.AddResponse(register_url_, /*head=*/nullptr,
                                  /*content=*/"",
                                  network::URLLoaderCompletionStatus(
                                      net::Error::ERR_INTERNET_DISCONNECTED));
  EXPECT_CALL(fcm_handler_, StartListening).Times(1);
  EXPECT_CALL(fcm_handler_, StopListening).Times(0);
  EXPECT_CALL(page_, OnInitReceiverError).WillOnce([&register_error_signal]() {
    register_error_signal.GetCallback().Run();
  });
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);
  EXPECT_TRUE(register_error_signal.Wait());

  EXPECT_CALL(page_, OnInitReceiverError).WillOnce([&offline_error_signal]() {
    offline_error_signal.GetCallback().Run();
  });
  SetDeviceOffline();
  EXPECT_TRUE(offline_error_signal.Wait());

  url_loader_factory_.AddResponse(register_url_.spec(),
                                  R"({"receiverId": "CD34"})");
  EXPECT_CALL(page_, OnInitReceiverInfo)
      .WillOnce([&on_init_receiver_info_future](
                    mojom::ReceiverInfoPtr receiver_info) {
        on_init_receiver_info_future.GetCallback().Run(
            std::move(receiver_info));
      });
  SetDeviceOnline();

  EXPECT_EQ(on_init_receiver_info_future.Take()->id, "CD34");
}

class BocaReceiverUntrustedPageHandlerNoActiveConnectionTest
    : public BocaReceiverUntrustedPageHandlerTest,
      public testing::WithParamInterface<std::string_view> {};

TEST_P(BocaReceiverUntrustedPageHandlerNoActiveConnectionTest,
       UpdateConnectionState) {
  // No active connection on the client.
  url_loader_factory_.AddResponse(get_connection_url_.spec(), "{}");
  boca::InvalidationsListener* listener = nullptr;
  EXPECT_CALL(fcm_handler_, AddListener)
      .WillOnce([&listener](boca::InvalidationsListener* listener_param) {
        listener = listener_param;
      });
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);

  // Simulate an invalidation with a given state.
  const std::string_view connection_state = GetParam();
  url_loader_factory_.AddResponse(
      get_connection_url_.spec(),
      CreateConnectionInfo(kConnectionId, connection_state));

  EXPECT_CALL(page_, OnConnectionClosed).Times(0);
  EXPECT_CALL(*remoting_client_, StopCrdClient).Times(0);

  listener->OnInvalidationReceived("payload");
  EXPECT_EQ(GetRequestBody(update_connection_url_), kDisconnectedPair);
}

INSTANTIATE_TEST_SUITE_P(All,
                         BocaReceiverUntrustedPageHandlerNoActiveConnectionTest,
                         testing::Values("STOP_REQUESTED",
                                         "CONNECTING",
                                         "CONNECTED"));

struct CrdStateTestCase {
  std::string test_name;
  boca::CrdConnectionState state;
  mojom::ConnectionClosedReason expected_reason;
  std::string_view expected_request_body;
};

class BocaReceiverUntrustedPageHandlerCrdStateTest
    : public BocaReceiverUntrustedPageHandlerTest,
      public testing::WithParamInterface<CrdStateTestCase> {};

TEST_P(BocaReceiverUntrustedPageHandlerCrdStateTest,
       CrdConnectionStateUpdated) {
  url_loader_factory_.AddResponse(get_connection_url_.spec(),
                                  CreateConnectionInfo(kConnectionId));
  boca::SpotlightCrdStateUpdatedCallback state_updated_cb;
  EXPECT_CALL(*remoting_client_,
              StartCrdClient(std::string(kConnectionCode), _, _, _, _))
      .WillOnce([&state_updated_cb](auto, auto, auto, auto,
                                    auto state_updated_cb_param) {
        state_updated_cb = std::move(state_updated_cb_param);
      });
  handler_ = std::make_unique<BocaReceiverUntrustedPageHandler>(
      page_.BindAndGetRemote(), &handler_delegate_);

  // Verify the first state update to CONNECTING.
  EXPECT_EQ(GetRequestBody(update_connection_url_), kConnectingPair);
  url_loader_factory_.SimulateResponseForPendingRequest(
      update_connection_url_.spec(), kConnectingPair);

  ASSERT_FALSE(state_updated_cb.is_null());
  base::test::TestFuture<mojom::ConnectionClosedReason>
      connection_closed_future;
  EXPECT_CALL(page_, OnConnectionClosed)
      .WillOnce(
          [&connection_closed_future](mojom::ConnectionClosedReason reason) {
            connection_closed_future.GetCallback().Run(reason);
          });
  EXPECT_CALL(*remoting_client_, StopCrdClient).Times(1);
  state_updated_cb.Run(GetParam().state);

  EXPECT_EQ(connection_closed_future.Get(), GetParam().expected_reason);
  EXPECT_EQ(GetRequestBody(update_connection_url_),
            GetParam().expected_request_body);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BocaReceiverUntrustedPageHandlerCrdStateTest,
    testing::ValuesIn<CrdStateTestCase>(
        {{.test_name = "Disconnected",
          .state = boca::CrdConnectionState::kDisconnected,
          .expected_reason =
              mojom::ConnectionClosedReason::kPresenterConnectionLost,
          .expected_request_body = kDisconnectedPair},
         {.test_name = "Timeout",
          .state = boca::CrdConnectionState::kTimeout,
          .expected_reason =
              mojom::ConnectionClosedReason::kPresenterConnectionLost,
          .expected_request_body = kDisconnectedPair},
         {.test_name = "Failed",
          .state = boca::CrdConnectionState::kFailed,
          .expected_reason = mojom::ConnectionClosedReason::kError,
          .expected_request_body = kErrorPair}}),
    [](const testing::TestParamInfo<
        BocaReceiverUntrustedPageHandlerCrdStateTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace ash::boca_receiver
