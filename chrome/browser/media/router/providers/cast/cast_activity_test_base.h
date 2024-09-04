// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_TEST_BASE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_TEST_BASE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/router/providers/cast/cast_activity.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "components/media_router/common/test/test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class MockCastSessionClient : public CastSessionClient {
 public:
  MockCastSessionClient(const std::string& client_id,
                        const url::Origin& origin,
                        content::FrameTreeNodeId tab_id);
  ~MockCastSessionClient() override;

  static const std::vector<MockCastSessionClient*>& instances() {
    return instances_;
  }

  MOCK_METHOD0(Init, mojom::RoutePresentationConnectionPtr());
  MOCK_METHOD1(SendMessageToClient,
               void(blink::mojom::PresentationConnectionMessagePtr message));
  MOCK_METHOD2(SendMediaMessageToClient,
               void(const base::Value::Dict& payload,
                    std::optional<int> request_id));
  MOCK_METHOD1(
      CloseConnection,
      void(blink::mojom::PresentationConnectionCloseReason close_reason));
  MOCK_METHOD0(TerminateConnection, void());
  MOCK_CONST_METHOD2(MatchesAutoJoinPolicy,
                     bool(url::Origin origin, content::FrameTreeNodeId tab_id));
  MOCK_METHOD3(SendErrorCodeToClient,
               void(int sequence_number,
                    CastInternalMessage::ErrorCode error_code,
                    std::optional<std::string> description));
  MOCK_METHOD2(SendErrorToClient,
               void(int sequence_number, base::Value::Dict error));
  MOCK_METHOD1(OnMessage,
               void(blink::mojom::PresentationConnectionMessagePtr message));
  MOCK_METHOD1(DidChangeState,
               void(blink::mojom::PresentationConnectionState state));
  MOCK_METHOD1(DidClose,
               void(blink::mojom::PresentationConnectionCloseReason reason));

 private:
  static std::vector<MockCastSessionClient*> instances_;
};

class MockCastActivityManager : public CastActivityManagerBase {
 public:
  MockCastActivityManager();
  ~MockCastActivityManager();

  MOCK_METHOD2(MakeResultCallbackForRoute,
               cast_channel::ResultCallback(
                   const std::string& route_id,
                   mojom::MediaRouteProvider::TerminateRouteCallback callback));
};

// Base class for testing subclasses of CastActivity.
class CastActivityTestBase : public testing::Test,
                             public CastSessionClientFactoryForTest {
 protected:
  static constexpr int kChannelId = 42;
  static const char* const kAppId;
  static const char* const kRouteId;
  static const char* const kSinkId;
  static const char* const kHashToken;

  CastActivityTestBase();
  ~CastActivityTestBase() override;

  void SetUp() override;
  void TearDown() override;

  // Run any pending events and verify expectations associated with them.
  void RunUntilIdle();

  // from CastSessionClientFactoryForTest
  std::unique_ptr<CastSessionClient> MakeClientForTest(
      const std::string& client_id,
      const url::Origin& origin,
      content::FrameTreeNodeId tab_id) override;

  // Adds a client to |activity| and returns a mock instance.
  MockCastSessionClient* AddMockClient(CastActivity* activity,
                                       const std::string& client_id,
                                       content::FrameTreeNodeId tab_id);

  // TODO(crbug.com/40623998): Factor out members also present in
  // CastActivityManagerTest.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  TestMediaSinkService media_sink_service_;
  cast_channel::MockCastSocketService socket_service_{
      task_environment_.GetMainThreadTaskRunner()};
  cast_channel::MockCastMessageHandler message_handler_{&socket_service_};
  CastSessionTracker session_tracker_{&media_sink_service_, &message_handler_,
                                      socket_service_.task_runner()};
  MediaSinkInternal sink_ = CreateCastSink(kChannelId);
  MockCastActivityManager manager_;
  raw_ptr<CastSession> session_ = nullptr;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_TEST_BASE_H_
