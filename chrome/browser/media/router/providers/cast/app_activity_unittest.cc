// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/app_activity.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_test_base.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "chrome/browser/media/router/providers/cast/test_util.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "components/media_router/common/test/test_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::IsJson;
using base::test::ParseJsonDict;
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

class AppActivityTest : public CastActivityTestBase {
 protected:
  void SetUp() override {
    CastActivityTestBase::SetUp();

    activity_ = std::make_unique<AppActivity>(
        MediaRoute(kRouteId, MediaSource("https://example.com/receiver.html"),
                   kSinkId, "", false),
        kAppId, &message_handler_, &session_tracker_);
  }

  void SetUpSession() { activity_->SetOrUpdateSession(*session_, sink_, ""); }

  const AppActivity::ClientMap& connected_clients() const {
    return activity_->connected_clients_;
  }

  MediaRoute& route() const { return activity_->route_; }

  MockCastSessionClient* AddMockClient(const std::string& client_id) {
    return CastActivityTestBase::AddMockClient(
        activity_.get(), client_id,
        content::FrameTreeNodeId(tab_id_counter_++));
  }

  int tab_id_counter_ = 239;  // Arbitrary number.
  std::unique_ptr<AppActivity> activity_;
};

TEST_F(AppActivityTest, SendAppMessageToReceiver) {
  // TODO(crbug.com/40623998): Test case where there is no session.
  // TODO(crbug.com/40623998): Test case where message has invalid namespace.

  EXPECT_CALL(message_handler_, SendAppMessage(kChannelId, _))
      .WillOnce(Return(cast_channel::Result::kFailed))
      .WillOnce(WithArg<1>(
          [](const openscreen::cast::proto::CastMessage& cast_message) {
            EXPECT_EQ("theClientId", cast_message.source_id());
            EXPECT_EQ("theTransportId", cast_message.destination_id());
            EXPECT_EQ("urn:x-cast:com.google.foo", cast_message.namespace_());
            EXPECT_TRUE(cast_message.has_payload_utf8());
            EXPECT_THAT(cast_message.payload_utf8(),
                        IsJson(R"({"foo": "bar"})"));
            EXPECT_FALSE(cast_message.has_payload_binary());
            return cast_channel::Result::kOk;
          }));

  std::unique_ptr<CastInternalMessage> message =
      CastInternalMessage::From(ParseJsonDict(R"({
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
            activity_->SendAppMessageToReceiver(*message));
  EXPECT_EQ(cast_channel::Result::kOk,
            activity_->SendAppMessageToReceiver(*message));
}

TEST_F(AppActivityTest, SendMediaRequestToReceiver) {
  // TODO(crbug.com/40623998): Test case where there is no session.

  const std::optional<int> request_id = 1234;

  EXPECT_CALL(
      message_handler_,
      SendMediaRequest(
          kChannelId,
          IsJson(
              R"({"sessionId": "theSessionId", "type": "theV2MessageType"})"),
          "theClientId", "theTransportId"))
      .WillOnce(Return(std::nullopt))
      .WillOnce(Return(request_id));

  std::unique_ptr<CastInternalMessage> message =
      CastInternalMessage::From(ParseJsonDict(R"({
    "type": "v2_message",
    "clientId": "theClientId",
    "sequenceNumber": 999,
    "message": {
      "type": "theV2MessageType",
      "sessionId": "theSessionId",
    },
  })"));

  SetUpSession();
  EXPECT_FALSE(activity_->SendMediaRequestToReceiver(*message));
  EXPECT_EQ(request_id, activity_->SendMediaRequestToReceiver(*message));
}

TEST_F(AppActivityTest, SendSetVolumeRequestToReceiver) {
  // TODO(crbug.com/40623998): Test case where no socket is found kChannelId.
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

  SetUpSession();
  std::unique_ptr<CastInternalMessage> message =
      CastInternalMessage::From(ParseJsonDict(R"({
    "type": "v2_message",
    "clientId": "theClientId",
    "sequenceNumber": 999,
    "message": {
      "type": "theV2MessageType",
      "sessionId": "theSessionId",
    },
  })"));
  activity_->SendSetVolumeRequestToReceiver(*message, callback.Get());
}

TEST_F(AppActivityTest, StopSessionOnReceiver) {
  const std::optional<std::string> client_id("theClientId");
  base::MockCallback<cast_channel::ResultCallback> callback;

  SetUpSession();
  EXPECT_CALL(message_handler_,
              StopSession(kChannelId, "theSessionId", client_id, _))
      .WillOnce(WithArg<3>([](cast_channel::ResultCallback callback) {
        std::move(callback).Run(cast_channel::Result::kOk);
      }));
  EXPECT_CALL(callback, Run(cast_channel::Result::kOk));
  activity_->StopSessionOnReceiver(client_id.value(), callback.Get());
}

TEST_F(AppActivityTest, SendStopSessionMessageToClients) {
  SetUpSession();
  auto* client = AddMockClient("theClientId");
  EXPECT_CALL(
      *client,
      SendMessageToClient(IsPresentationConnectionMessage(
          CreateReceiverActionStopMessage("theClientId", sink_, kHashToken)
              ->get_message())));
  activity_->SendStopSessionMessageToClients(kHashToken);
}

TEST_F(AppActivityTest, HandleLeaveSession) {
  SetUpSession();
  AddMockClient("theClientId");
  AddMockClient("leaving");
  AddMockClient("keeping");
  for (auto* client : MockCastSessionClient::instances()) {
    const bool is_leaving = client->client_id() == "leaving";
    EXPECT_CALL(*client,
                CloseConnection(PresentationConnectionCloseReason::CLOSED))
        .Times(is_leaving ? 1 : 0);
    EXPECT_CALL(*client, MatchesAutoJoinPolicy)
        .WillRepeatedly(Return(is_leaving));
  }
  activity_->HandleLeaveSession("theClientId");
  EXPECT_THAT(connected_clients(),
              UnorderedElementsAre(Pair("theClientId", _), Pair("keeping", _)));
}

TEST_F(AppActivityTest, SendMessageToClientInvalid) {
  SetUpSession();

  // An invalid client ID is ignored.
  activity_->SendMessageToClient("theClientId", nullptr);
}

TEST_F(AppActivityTest, SendMessageToClient) {
  SetUpSession();

  PresentationConnectionMessagePtr message =
      PresentationConnectionMessage::NewMessage("\"theMessage\"");
  auto* message_ptr = message.get();
  auto* client = AddMockClient("theClientId");
  EXPECT_CALL(*client, SendMessageToClient).WillOnce([=](auto arg) {
    EXPECT_EQ(message_ptr, arg.get());
  });
  activity_->SendMessageToClient("theClientId", std::move(message));
}

TEST_F(AppActivityTest, AddRemoveClient) {
  // TODO(crbug.com/40623998): Check value returned by AddClient().

  // Adding clients works as expected.
  ASSERT_TRUE(connected_clients().empty());
  ASSERT_FALSE(route().is_local());
  AddMockClient("theClientId1");
  // Check that adding a client causes the route to become local.
  EXPECT_TRUE(route().is_local());
  EXPECT_THAT(connected_clients(),
              UnorderedElementsAre(Pair("theClientId1", _)));
  AddMockClient("theClientId2");
  EXPECT_TRUE(route().is_local());
  EXPECT_THAT(
      connected_clients(),
      UnorderedElementsAre(Pair("theClientId1", _), Pair("theClientId2", _)));

  // Removing a non-existant client is a no-op.
  activity_->RemoveClient("noSuchClient");
  EXPECT_THAT(
      connected_clients(),
      UnorderedElementsAre(Pair("theClientId1", _), Pair("theClientId2", _)));

  // Removing clients works as expected.
  activity_->RemoveClient("theClientId1");
  EXPECT_THAT(connected_clients(),
              UnorderedElementsAre(Pair("theClientId2", _)));
  activity_->RemoveClient("theClientId2");
  EXPECT_TRUE(connected_clients().empty());
}

TEST_F(AppActivityTest, SetOrUpdateSession) {
  AddMockClient("theClientId1");
  AddMockClient("theClientId2");

  ASSERT_EQ(std::nullopt, activity_->session_id());
  route().set_description("");
  for (auto* client : MockCastSessionClient::instances()) {
    EXPECT_CALL(*client, SendMessageToClient).Times(0);
  }
  ASSERT_EQ(session_->GetRouteDescription(), "theStatusText");
  activity_->SetOrUpdateSession(*session_, sink_, "");
  EXPECT_EQ("theStatusText", route().description());
  EXPECT_EQ("theSessionId", activity_->session_id());

  route().set_description("");
  for (auto* client : MockCastSessionClient::instances()) {
    // TODO(crbug.com/1291744): Check argument of SendMessageToClient.
    EXPECT_CALL(*client, SendMessageToClient).Times(1);
  }
  activity_->SetOrUpdateSession(*session_, sink_, "theHashToken");
  EXPECT_EQ("theStatusText", route().description());
  EXPECT_EQ("theSessionId", activity_->session_id());
}

TEST_F(AppActivityTest, ClosePresentationConnections) {
  constexpr auto reason = PresentationConnectionCloseReason::CONNECTION_ERROR;

  AddMockClient("theClientId1");
  AddMockClient("theClientId2");
  for (auto* client : MockCastSessionClient::instances()) {
    EXPECT_CALL(*client, CloseConnection(reason));
  }
  activity_->ClosePresentationConnections(reason);
}

TEST_F(AppActivityTest, TerminatePresentationConnections) {
  AddMockClient("theClientId1");
  AddMockClient("theClientId2");
  ASSERT_FALSE(MockCastSessionClient::instances().empty());
  for (auto* client : MockCastSessionClient::instances()) {
    EXPECT_CALL(*client, TerminateConnection());
  }
  activity_->TerminatePresentationConnections();
}

TEST_F(AppActivityTest, OnAppMessage) {
  SetUpSession();

  auto* client = AddMockClient("theClientId");
  auto message = cast_channel::CreateCastMessage(
      "urn:x-cast:com.google.foo", base::Value(base::Value::Dict()), "sourceId",
      "theClientId");
  EXPECT_CALL(*client,
              SendMessageToClient(IsPresentationConnectionMessage(
                  CreateAppMessage("theSessionId", "theClientId", message)
                      ->get_message())));
  activity_->OnAppMessage(message);
}

TEST_F(AppActivityTest, OnAppMessageAllClients) {
  SetUpSession();

  auto* client1 = AddMockClient("theClientId1");
  auto* client2 = AddMockClient("theClientId2");
  auto message = cast_channel::CreateCastMessage(
      "urn:x-cast:com.google.foo", base::Value(base::Value::Dict()), "sourceId",
      "*");
  EXPECT_CALL(*client1,
              SendMessageToClient(IsPresentationConnectionMessage(
                  CreateAppMessage("theSessionId", "theClientId1", message)
                      ->get_message())));
  EXPECT_CALL(*client2,
              SendMessageToClient(IsPresentationConnectionMessage(
                  CreateAppMessage("theSessionId", "theClientId2", message)
                      ->get_message())));
  activity_->OnAppMessage(message);
}

TEST_F(AppActivityTest, CloseConnectionOnReceiver) {
  SetUpSession();
  AddMockClient("theClientId1");

  EXPECT_CALL(message_handler_, CloseConnection(kChannelId, "theClientId1",
                                                session_->destination_id()));
  activity_->CloseConnectionOnReceiver(
      "theClientId1", blink::mojom::PresentationConnectionCloseReason::CLOSED);
}

TEST_F(AppActivityTest, RemoveConnectionOnReceiver) {
  SetUpSession();
  AddMockClient("theClientId1");

  // If the close reason is not `CLOSED`, then we call RemoveConnection()
  // instead of CloseConnection() to avoid sending a close request to the
  // receiver.
  EXPECT_CALL(message_handler_, RemoveConnection(kChannelId, "theClientId1",
                                                 session_->destination_id()));
  activity_->CloseConnectionOnReceiver(
      "theClientId1",
      blink::mojom::PresentationConnectionCloseReason::WENT_AWAY);
}

TEST_F(AppActivityTest, ForwardInternalMediaMessage) {
  const std::string client_id = "theClientId";
  base::Value::Dict payload = ParseJsonDict(R"({
    "type": "v2_message",
    "clientId": "theClientId",
    "message": {
      "type": "INVALID_REQUEST",
      "sessionId": "theSessionId",
    },
  })");
  SetUpSession();
  MockCastSessionClient* client = AddMockClient(client_id);

  EXPECT_CALL(*client, SendMediaMessageToClient);
  activity_->OnInternalMessage(cast_channel::InternalMessage(
      cast_channel::CastMessageType::kInvalidRequest, "theSourceId", client_id,
      cast_channel::kMediaNamespace, std::move(payload)));
}

TEST_F(AppActivityTest, IgnoreInternalMediaStatusMessage) {
  const std::string client_id = "theClientId";
  base::Value::Dict media_status_payload = ParseJsonDict(R"({
    "type": "v2_message",
    "clientId": "theClientId",
    "message": {
      "type": "MEDIA_STATUS",
      "sessionId": "theSessionId",
    },
  })");
  SetUpSession();
  MockCastSessionClient* client = AddMockClient(client_id);

  // OnInternalMessage() should ignore `kMediaStatus` messages because they're
  // handled elsewhere.
  EXPECT_CALL(*client, SendMediaMessageToClient).Times(0);
  activity_->OnInternalMessage(cast_channel::InternalMessage(
      cast_channel::CastMessageType::kMediaStatus, "theSourceId", client_id,
      cast_channel::kMediaNamespace, std::move(media_status_payload)));
}

}  // namespace media_router
