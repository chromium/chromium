// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_activity_test_base.h"

#include <algorithm>
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
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "chrome/browser/media/router/providers/cast/test_util.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "components/media_router/common/test/test_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::ParseJsonDict;
using testing::NiceMock;

namespace media_router {

MockCastSessionClient::MockCastSessionClient(const std::string& client_id,
                                             const url::Origin& origin,
                                             content::FrameTreeNodeId tab_id)
    : CastSessionClient(client_id, origin, tab_id) {
  instances_.push_back(this);
}

MockCastSessionClient::~MockCastSessionClient() {
  std::erase(instances_, this);
}

std::vector<MockCastSessionClient*> MockCastSessionClient::instances_;

MockCastActivityManager::MockCastActivityManager() = default;

MockCastActivityManager::~MockCastActivityManager() = default;

const char* const CastActivityTestBase::kAppId = "theAppId";
const char* const CastActivityTestBase::kRouteId = "theRouteId";
const char* const CastActivityTestBase::kSinkId = "cast:id42";
const char* const CastActivityTestBase::kHashToken = "dummyHashToken";

CastActivityTestBase::CastActivityTestBase() = default;

CastActivityTestBase::~CastActivityTestBase() = default;

void CastActivityTestBase::SetUp() {
  ASSERT_TRUE(MockCastSessionClient::instances().empty());

  media_sink_service_.AddOrUpdateSink(sink_);
  ASSERT_EQ(kSinkId, sink_.id());

  media_sink_service_.AddOrUpdateSink(sink_);
  ASSERT_EQ(kSinkId, sink_.id());

  CastActivity::SetClientFactoryForTest(this);

  std::unique_ptr<CastSession> session =
      CastSession::From(sink_, ParseJsonDict(R"({
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
  session_tracker_.SetSessionForTest(kSinkId, std::move(session));
}

void CastActivityTestBase::TearDown() {
  RunUntilIdle();
  CastActivity::SetClientFactoryForTest(nullptr);
}

void CastActivityTestBase::RunUntilIdle() {
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&socket_service_);
  testing::Mock::VerifyAndClearExpectations(&message_handler_);
  testing::Mock::VerifyAndClearExpectations(&manager_);
  for (const auto* client : MockCastSessionClient::instances())
    testing::Mock::VerifyAndClearExpectations(&client);
}

std::unique_ptr<CastSessionClient> CastActivityTestBase::MakeClientForTest(
    const std::string& client_id,
    const url::Origin& origin,
    content::FrameTreeNodeId tab_id) {
  return std::make_unique<NiceMock<MockCastSessionClient>>(client_id, origin,
                                                           tab_id);
}

MockCastSessionClient* CastActivityTestBase::AddMockClient(
    CastActivity* activity,
    const std::string& client_id,
    content::FrameTreeNodeId tab_id) {
  CastMediaSource source("dummySourceId", std::vector<CastAppInfo>());
  source.set_client_id(client_id);
  activity->AddClient(source, url::Origin(), tab_id);
  return MockCastSessionClient::instances().back();
}

}  // namespace media_router
