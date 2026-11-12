// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/test/test_future.h"
#include "chrome/browser/optimization_guide/mock_remote_model_executor.h"
#include "chrome/browser/ttc/app/app_browser_test_base.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/states.h"
#include "chrome/browser/ttc/core/ttc_page_context_monitor.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/streaming/ttc.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"
#include "url/gurl.h"

namespace ttc {

namespace {

namespace proto = optimization_guide::proto;

std::vector<std::string> CollectText(const proto::ContentNode& node) {
  std::vector<std::string> text;
  if (node.content_attributes().has_text_data()) {
    text.push_back(node.content_attributes().text_data().text_content());
  }

  for (const proto::ContentNode& child : node.children_nodes()) {
    std::vector<std::string> child_text = CollectText(child);
    text.insert(text.end(), std::make_move_iterator(child_text.begin()),
                std::make_move_iterator(child_text.end()));
  }

  return text;
}

class TtcPageContextProtocolBrowserTest : public AppBrowserTestBase {
 public:
  TtcPageContextProtocolBrowserTest() = default;
  ~TtcPageContextProtocolBrowserTest() override = default;

  void StartSession() {
    ON_CALL(*model_execution_session(), Send)
        .WillByDefault([callback = context_updates_.GetRepeatingCallback()](
                           const google::protobuf::MessageLite& message) {
          proto::TtcClientFrame frame;
          CHECK(frame.ParseFromString(message.SerializeAsString()));
          if (frame.has_context_update()) {
            callback.Run(frame.context_update());
          }
        });

    StartSessionAndConnectBackend();
    RespondSessionStatus(proto::SessionStatusResponse::STATE_CONNECTED,
                         "session_1");
    ASSERT_EQ(session_controller()->GetSessionLifecycle(),
              SessionLifecycle::kLive);
  }

  void RespondUserTurnState(proto::UserTurnStateUpdate::State state) {
    proto::TtcServerFrame frame;
    frame.mutable_user_turn_state_update()->set_state(state);
    SimulateResponse(frame);
  }

  void RespondModelTurnState(proto::ModelTurnStateUpdate::State state,
                             int64_t generation_id) {
    proto::TtcServerFrame frame;
    proto::ModelTurnStateUpdate* update =
        frame.mutable_model_turn_state_update();
    update->set_state(state);
    update->set_generation_id(generation_id);
    SimulateResponse(frame);
  }

  proto::ContextUpdate TakeNextContextUpdate() {
    return context_updates_.Take();
  }

  TtcPageContextMonitor* page_context_monitor() {
    return session_controller()
               ? session_controller()->page_context_monitor_for_testing()
               : nullptr;
  }

 private:
  base::test::TestFuture<proto::ContextUpdate> context_updates_{
      base::test::TestFutureMode::kQueue};
};

IN_PROC_BROWSER_TEST_F(TtcPageContextProtocolBrowserTest,
                       NavigationSendsContextUpdate) {
  const GURL first_url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), first_url));

  StartSession();

  proto::ContextUpdate initial_update = TakeNextContextUpdate();
  EXPECT_EQ(initial_update.active_tab().url(), first_url.spec());
  EXPECT_EQ(initial_update.active_tab().title(), "OK");
  ASSERT_TRUE(initial_update.has_annotated_page_content());
  EXPECT_THAT(CollectText(initial_update.annotated_page_content().root_node()),
              testing::Contains(testing::HasSubstr("Non empty simple page")));

  const GURL second_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), second_url));

  proto::ContextUpdate navigation_update = TakeNextContextUpdate();
  EXPECT_EQ(navigation_update.active_tab().url(), second_url.spec());
  EXPECT_EQ(navigation_update.active_tab().title(), "Title Of Awesomeness");
  ASSERT_TRUE(navigation_update.has_annotated_page_content());
  EXPECT_THAT(
      CollectText(navigation_update.annotated_page_content().root_node()),
      testing::Contains(testing::HasSubstr("This page has a title.")));
}

IN_PROC_BROWSER_TEST_F(TtcPageContextProtocolBrowserTest,
                       UserTurnStartSendsRefreshedContextUpdate) {
  const GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  StartSession();

  proto::ContextUpdate initial_update = TakeNextContextUpdate();
  EXPECT_EQ(initial_update.active_tab().url(), url.spec());
  ASSERT_TRUE(initial_update.has_annotated_page_content());
  EXPECT_THAT(CollectText(initial_update.annotated_page_content().root_node()),
              testing::Contains(testing::HasSubstr("Non empty simple page")));

  ASSERT_NE(page_context_monitor(), nullptr);

  ASSERT_TRUE(content::ExecJs(
      web_contents(), "document.body.textContent = 'First dynamic update';"));

  EXPECT_FALSE(page_context_monitor()->IsFetchingForTesting());
  RespondUserTurnState(proto::UserTurnStateUpdate::TURN_STARTED);
  EXPECT_TRUE(page_context_monitor()->IsFetchingForTesting());

  proto::ContextUpdate first_turn_update = TakeNextContextUpdate();
  EXPECT_FALSE(page_context_monitor()->IsFetchingForTesting());
  EXPECT_EQ(first_turn_update.active_tab().url(), url.spec());
  ASSERT_TRUE(first_turn_update.has_annotated_page_content());
  EXPECT_THAT(
      CollectText(first_turn_update.annotated_page_content().root_node()),
      testing::Contains(testing::HasSubstr("First dynamic update")));

  RespondUserTurnState(proto::UserTurnStateUpdate::TURN_COMPLETED);
  EXPECT_FALSE(page_context_monitor()->IsFetchingForTesting());

  ASSERT_TRUE(content::ExecJs(
      web_contents(), "document.body.textContent = 'Second dynamic update';"));

  constexpr int64_t kGenerationId = 1;
  RespondModelTurnState(proto::ModelTurnStateUpdate::TURN_STARTED,
                        kGenerationId);
  EXPECT_FALSE(page_context_monitor()->IsFetchingForTesting());
  RespondModelTurnState(proto::ModelTurnStateUpdate::GENERATION_COMPLETED,
                        kGenerationId);
  EXPECT_FALSE(page_context_monitor()->IsFetchingForTesting());
  RespondModelTurnState(proto::ModelTurnStateUpdate::TURN_COMPLETED,
                        kGenerationId);
  EXPECT_FALSE(page_context_monitor()->IsFetchingForTesting());

  RespondUserTurnState(proto::UserTurnStateUpdate::TURN_STARTED);
  EXPECT_TRUE(page_context_monitor()->IsFetchingForTesting());

  proto::ContextUpdate second_turn_update = TakeNextContextUpdate();
  EXPECT_FALSE(page_context_monitor()->IsFetchingForTesting());
  EXPECT_EQ(second_turn_update.active_tab().url(), url.spec());
  ASSERT_TRUE(second_turn_update.has_annotated_page_content());
  EXPECT_THAT(
      CollectText(second_turn_update.annotated_page_content().root_node()),
      testing::AllOf(
          testing::Contains(testing::HasSubstr("Second dynamic update")),
          testing::Not(
              testing::Contains(testing::HasSubstr("First dynamic update")))));
}

}  // namespace

}  // namespace ttc
