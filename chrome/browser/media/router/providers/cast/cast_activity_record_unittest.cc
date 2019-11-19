// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_activity_record.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "chrome/browser/media/router/providers/cast/test_util.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/common/media_router/test/test_helper.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::IsJson;
using base::test::ParseJson;
using blink::mojom::PresentationConnectionCloseReason;
using blink::mojom::PresentationConnectionMessage;
using blink::mojom::PresentationConnectionMessagePtr;
using testing::_;
using testing::Pair;
using testing::Pointee;
using testing::Return;
using testing::UnorderedElementsAre;
using testing::WithArg;
using testing::WithArgs;

namespace media_router {

namespace {

constexpr int kChannelId = 42;
constexpr char kAppId[] = "theAppId";
constexpr char kRouteId[] = "theRouteId";
constexpr char kSinkId[] = "cast:<id42>";

class MockCastSessionClientImpl : public CastSessionClient {
 public:
  using CastSessionClient::CastSessionClient;

  MOCK_METHOD0(Init, mojom::RoutePresentationConnectionPtr());
  MOCK_METHOD1(SendMessageToClient,
               void(blink::mojom::PresentationConnectionMessagePtr message));
  MOCK_METHOD2(SendMediaStatusToClient,
               void(const base::Value& media_status,
                    base::Optional<int> request_id));
  MOCK_METHOD1(
      CloseConnection,
      void(blink::mojom::PresentationConnectionCloseReason close_reason));
  MOCK_METHOD0(TerminateConnection, void());
  MOCK_CONST_METHOD2(MatchesAutoJoinPolicy,
                     bool(url::Origin origin, int tab_id));
  MOCK_METHOD3(SendErrorCodeToClient,
               void(int sequence_number,
                    CastInternalMessage::ErrorCode error_code,
                    base::Optional<std::string> description));
  MOCK_METHOD2(SendErrorToClient, void(int sequence_number, base::Value error));
  MOCK_METHOD1(OnMessage,
               void(blink::mojom::PresentationConnectionMessagePtr message));
  MOCK_METHOD1(DidChangeState,
               void(blink::mojom::PresentationConnectionState state));
  MOCK_METHOD1(DidClose,
               void(blink::mojom::PresentationConnectionCloseReason reason));
};

class MockCastActivityManager : public CastActivityManagerBase {
 public:
  MOCK_METHOD2(MakeResultCallbackForRoute,
               cast_channel::ResultCallback(
                   const std::string& route_id,
                   mojom::MediaRouteProvider::TerminateRouteCallback callback));
};

}  // namespace

class CastActivityRecordTest : public testing::Test,
                               public CastSessionClientFactoryForTest {
 public:
  CastActivityRecordTest() {}

  ~CastActivityRecordTest() override = default;

  void SetUp() override {
    media_sink_service_.AddOrUpdateSink(sink_);
    ASSERT_EQ(kSinkId, sink_.id());

    CastActivityRecord::SetClientFactoryForTest(this);

    session_tracker_.reset(
        new CastSessionTracker(&media_sink_service_, &message_handler_,
                               socket_service_.task_runner()));

    MediaRoute route;
    route.set_media_route_id(kRouteId);
    route.set_media_sink_id(kSinkId);
    record_.reset(new CastActivityRecord(route, kAppId, &media_sink_service_,
                                         &message_handler_,
                                         session_tracker_.get(), &manager_));

    std::unique_ptr<CastSession> session =
        CastSession::From(sink_, ParseJson(R"({
        "applications": [{
          "appId": "theAppId",
          "displayName": "App display name",
          "namespaces": [
            {"name": "urn:x-cast:com.google.cast.media"},
            {"name": "urn:x-cast:com.google.foo"}
          ],
          "sessionId": "theSessionId",
          "statusText": "theStatusText",
          "transportId": "theTransportId"
        }]
      })"));
    ASSERT_EQ("theSessionId", session->session_id());
    session_ = session.get();
    session_tracker_->SetSessionForTest(kSinkId, std::move(session));
  }

  void TearDown() override {
    RunUntilIdle();
    CastActivityRecord::SetClientFactoryForTest(nullptr);
  }

  std::unique_ptr<CastSessionClient> MakeClientForTest(
      const std::string& client_id,
      const url::Origin& origin,
      int tab_id) override {
    auto client =
        std::make_unique<MockCastSessionClientImpl>(client_id, origin, tab_id);
    clients_.push_back(client.get());
    return std::move(client);
  }

 protected:
  void SetUpSession() { record_->SetOrUpdateSession(*session_, sink_, ""); }

  // Run any pending events and verify expectations associated with them.
  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&socket_service_);
    testing::Mock::VerifyAndClearExpectations(&message_handler_);
    testing::Mock::VerifyAndClearExpectations(&manager_);
  }

  MediaRoute& route() { return record_->route_; }

  MockCastSessionClientImpl* AddMockClient(const std::string& client_id) {
    CastMediaSource source("dummySourceId", std::vector<CastAppInfo>());
    source.set_client_id(client_id);
    record_->AddClient(source, url::Origin(), tab_id_counter_++);
    return clients_.back();
  }

  int tab_id_counter_ = 239;  // Arbitrary number.
  std::vector<MockCastSessionClientImpl*> clients_;

  // TODO(crbug.com/954797): Factor out members also present in
  // CastActivityManagerTest.
  content::BrowserTaskEnvironment task_environment_;
  MediaSinkInternal sink_ = CreateCastSink(kChannelId);
  cast_channel::MockCastSocketService socket_service_{
      base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})};
  cast_channel::MockCastMessageHandler message_handler_{&socket_service_};
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  TestMediaSinkService media_sink_service_;
  std::unique_ptr<CastSessionTracker> session_tracker_;
  MockCastActivityManager manager_;
  CastSession* session_ = nullptr;
  std::unique_ptr<CastActivityRecord> record_;
};

TEST_F(CastActivityRecordTest, SendAppMessageToReceiver) {
  // TODO(crbug.com/954797): Test case where there is no session.
  // TODO(crbug.com/954797): Test case where message has invalid namespace.

  EXPECT_CALL(message_handler_, SendAppMessage(kChannelId, _))
      .WillOnce(Return(cast_channel::Result::kFailed))
      .WillOnce(WithArg<1>([](const cast_channel::CastMessage& cast_message) {
        EXPECT_EQ("theClientId", cast_message.source_id());
        EXPECT_EQ("theTransportId", cast_message.destination_id());
        EXPECT_EQ("urn:x-cast:com.google.foo", cast_message.namespace_());
        EXPECT_TRUE(cast_message.has_payload_utf8());
        EXPECT_THAT(cast_message.payload_utf8(), IsJson(R"({"foo": "bar"})"));
        EXPECT_FALSE(cast_message.has_payload_binary());
        return cast_channel::Result::kOk;
      }));

  std::unique_ptr<CastInternalMessage> message =
      CastInternalMessage::From(ParseJson(R"({
    "type": "app_message",
    "clientId": "theClientId",
    "sequenceNumber": 999,
    "message": {
      "namespaceName": "urn:x-cast:com.google.foo",
      "sessionId": "theSessionId",
      "message": { "foo": "bar" },
    },
  })"));

  SetUpSession();
  EXPECT_EQ(cast_channel::Result::kFailed,
            record_->SendAppMessageToReceiver(*message));
  EXPECT_EQ(cast_channel::Result::kOk,
            record_->SendAppMessageToReceiver(*message));
}

TEST_F(CastActivityRecordTest, SendMediaRequestToReceiver) {
  // TODO(crbug.com/954797): Test case where there is no session.

  const base::Optional<int> request_id = 1234;

  EXPECT_CALL(
      message_handler_,
      SendMediaRequest(
          kChannelId,
          IsJson(
              R"({"sessionId": "theSessionId", "type": "theV2MessageType"})"),
          "theClientId", "theTransportId"))
      .WillOnce(Return(base::nullopt))
      .WillOnce(Return(request_id));

  std::unique_ptr<CastInternalMessage> message =
      CastInternalMessage::From(ParseJson(R"({
    "type": "v2_message",
    "clientId": "theClientId",
    "sequenceNumber": 999,
    "message": {
      "type": "theV2MessageType",
      "sessionId": "theSessionId",
    },
  })"));

  SetUpSession();
  EXPECT_FALSE(record_->SendMediaRequestToReceiver(*message));
  EXPECT_EQ(request_id, record_->SendMediaRequestToReceiver(*message));
}

TEST_F(CastActivityRecordTest, SendSetVolumeRequestToReceiver) {
  // TODO(crbug.com/954797): Test case where no socket is found kChannelId.

  EXPECT_CALL(
      message_handler_,
      SendSetVolumeRequest(
          kChannelId,
          IsJson(
              R"({"sessionId": "theSessionId", "type": "theV2MessageType"})"),
          "theClientId", _))
      .WillOnce(WithArg<3>([](cast_channel::ResultCallback callback) {
        std::move(callback).Run(cast_channel::Result::kOk);
        return cast_channel::Result::kOk;
      }));

  base::MockCallback<cast_channel::ResultCallback> callback;
  EXPECT_CALL(callback, Run(cast_channel::Result::kOk));

  std::unique_ptr<CastInternalMessage> message =
      CastInternalMessage::From(ParseJson(R"({
    "type": "v2_message",
    "clientId": "theClientId",
    "sequenceNumber": 999,
    "message": {
      "type": "theV2MessageType",
      "sessionId": "theSessionId",
    },
  })"));
  record_->SendSetVolumeRequestToReceiver(*message, callback.Get());
}

TEST_F(CastActivityRecordTest, SendStopSessionMessageToReceiver) {
  const base::Optional<std::string> client_id("theClientId");

  EXPECT_CALL(message_handler_,
              StopSession(kChannelId, "theSessionId", client_id, _))
      .WillOnce(WithArg<3>([](cast_channel::ResultCallback callback) {
        std::move(callback).Run(cast_channel::Result::kFailed);
      }));

  EXPECT_CALL(manager_, MakeResultCallbackForRoute(kRouteId, _))
      .WillOnce(WithArg<1>(
          [](mojom::MediaRouteProvider::TerminateRouteCallback callback) {
            return base::BindOnce(
                [](mojom::MediaRouteProvider::TerminateRouteCallback callback,
                   cast_channel::Result result) {
                  EXPECT_EQ(cast_channel::Result::kFailed, result);
                  std::move(callback).Run(
                      base::Optional<std::string>("theErrorText"),
                      RouteRequestResult::INCOGNITO_MISMATCH);
                },
                std::move(callback));
          }));

  base::MockCallback<mojom::MediaRouteProvider::TerminateRouteCallback>
      callback;
  EXPECT_CALL(callback, Run(base::Optional<std::string>("theErrorText"),
                            RouteRequestResult::INCOGNITO_MISMATCH));

  SetUpSession();
  record_->SendStopSessionMessageToReceiver(client_id, "dummyHashToken",
                                            callback.Get());
}

TEST_F(CastActivityRecordTest, HandleLeaveSession) {
  SetUpSession();
  AddMockClient("theClientId");
  AddMockClient("leaving");
  AddMockClient("keeping");
  for (auto* client : clients_) {
    const bool is_leaving = client->client_id() == "leaving";
    EXPECT_CALL(*client,
                CloseConnection(PresentationConnectionCloseReason::CLOSED))
        .Times(is_leaving ? 1 : 0);
    EXPECT_CALL(*client, MatchesAutoJoinPolicy)
        .WillRepeatedly(Return(is_leaving));
  }
  record_->HandleLeaveSession("theClientId");
  EXPECT_THAT(record_->connected_clients(),
              UnorderedElementsAre(Pair("theClientId", _), Pair("keeping", _)));
}

TEST_F(CastActivityRecordTest, SendMessageToClientInvalid) {
  SetUpSession();

  // An invalid client ID is ignored.
  record_->SendMessageToClient("theClientId", nullptr);
}

TEST_F(CastActivityRecordTest, SendMessageToClient) {
  SetUpSession();

  PresentationConnectionMessagePtr message =
      PresentationConnectionMessage::NewMessage("\"theMessage\"");
  auto* message_ptr = message.get();
  auto* client = AddMockClient("theClientId");
  EXPECT_CALL(*client, SendMessageToClient).WillOnce([=](auto arg) {
    EXPECT_EQ(message_ptr, arg.get());
  });
  record_->SendMessageToClient("theClientId", std::move(message));
}

TEST_F(CastActivityRecordTest, AddRemoveClient) {
  // TODO(crbug.com/954797): Check value returned by AddClient().

  // Adding clients works as expected.
  ASSERT_TRUE(record_->connected_clients().empty());
  ASSERT_FALSE(route().is_local());
  AddMockClient("theClientId1");
  // Check that adding a client causes the route to become local.
  EXPECT_TRUE(route().is_local());
  EXPECT_THAT(record_->connected_clients(),
              UnorderedElementsAre(Pair("theClientId1", _)));
  AddMockClient("theClientId2");
  EXPECT_TRUE(route().is_local());
  EXPECT_THAT(
      record_->connected_clients(),
      UnorderedElementsAre(Pair("theClientId1", _), Pair("theClientId2", _)));

  // Removing a non-existant client is a no-op.
  record_->RemoveClient("noSuchClient");
  EXPECT_THAT(
      record_->connected_clients(),
      UnorderedElementsAre(Pair("theClientId1", _), Pair("theClientId2", _)));

  // Removing clients works as expected.
  record_->RemoveClient("theClientId1");
  EXPECT_THAT(record_->connected_clients(),
              UnorderedElementsAre(Pair("theClientId2", _)));
  record_->RemoveClient("theClientId2");
  EXPECT_TRUE(record_->connected_clients().empty());
}

TEST_F(CastActivityRecordTest, SetOrUpdateSession) {
  AddMockClient("theClientId1");
  AddMockClient("theClientId2");

  ASSERT_EQ(base::nullopt, record_->session_id());
  route().set_description("");
  for (auto* client : clients_) {
    EXPECT_CALL(*client, SendMessageToClient).Times(0);
  }
  record_->SetOrUpdateSession(*session_, sink_, "");
  EXPECT_EQ("theStatusText", route().description());
  EXPECT_EQ("theSessionId", record_->session_id());

  route().set_description("");
  for (auto* client : clients_) {
    // TODO(jrw): Check argument of SendMessageToClient.
    EXPECT_CALL(*client, SendMessageToClient).Times(1);
  }
  record_->SetOrUpdateSession(*session_, sink_, "theHashToken");
  EXPECT_EQ("theStatusText", route().description());
  EXPECT_EQ("theSessionId", record_->session_id());
}

TEST_F(CastActivityRecordTest, ClosePresentationConnections) {
  constexpr auto reason = PresentationConnectionCloseReason::CONNECTION_ERROR;

  AddMockClient("theClientId1");
  AddMockClient("theClientId2");
  for (auto* client : clients_) {
    EXPECT_CALL(*client, CloseConnection(reason));
  }
  record_->ClosePresentationConnections(reason);
}

TEST_F(CastActivityRecordTest, TerminatePresentationConnections) {
  AddMockClient("theClientId1");
  AddMockClient("theClientId2");
  for (auto* client : clients_) {
    EXPECT_CALL(*client, TerminateConnection());
  }
  record_->TerminatePresentationConnections();
}

TEST_F(CastActivityRecordTest, OnAppMessage) {
  SetUpSession();

  auto* client = AddMockClient("theClientId");
  auto message = cast_channel::CreateCastMessage(
      "urn:x-cast:com.google.foo", base::Value(base::Value::Type::DICTIONARY),
      "sourceId", "theClientId");
  EXPECT_CALL(*client,
              SendMessageToClient(IsPresentationConnectionMessage(
                  CreateAppMessage("theSessionId", "theClientId", message)
                      ->get_message())));
  record_->OnAppMessage(message);
}

TEST_F(CastActivityRecordTest, OnAppMessageAllClients) {
  SetUpSession();

  auto* client1 = AddMockClient("theClientId1");
  auto* client2 = AddMockClient("theClientId2");
  auto message = cast_channel::CreateCastMessage(
      "urn:x-cast:com.google.foo", base::Value(base::Value::Type::DICTIONARY),
      "sourceId", "*");
  EXPECT_CALL(*client1,
              SendMessageToClient(IsPresentationConnectionMessage(
                  CreateAppMessage("theSessionId", "theClientId1", message)
                      ->get_message())));
  EXPECT_CALL(*client2,
              SendMessageToClient(IsPresentationConnectionMessage(
                  CreateAppMessage("theSessionId", "theClientId2", message)
                      ->get_message())));
  record_->OnAppMessage(message);
}

}  // namespace media_router
