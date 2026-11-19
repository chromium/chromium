// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/critical_actions/critical_action_factory.h"
#include "chrome/browser/glic/actor/new_glic_actor_functional_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace glic::actor {
namespace {

using ::testing::ElementsAre;

// Records the conversation anchor updates that Glic sends to the
// `CriticalActionService`, as "set:<task ID>:<anchor>".
class AnchorRecordingCriticalActionService
    : public critical_actions::CriticalActionService {
 public:
  explicit AnchorRecordingCriticalActionService(const base::FilePath& db_path)
      : critical_actions::CriticalActionService(
            db_path,
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                 base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}
  ~AnchorRecordingCriticalActionService() override = default;

  void SetActorTaskConversationAnchorId(
      std::string_view actor_task_id,
      std::string_view conversation_anchor_id) override {
    anchor_updates_.push_back(
        base::StrCat({"set:", actor_task_id, ":", conversation_anchor_id}));
  }

  const std::vector<std::string>& anchor_updates() const {
    return anchor_updates_;
  }

 private:
  std::vector<std::string> anchor_updates_;
};

// Tests that the turn ID passed to `performActions()` reaches the
// `CriticalActionService` once the actor accepts the actions. Which calls the
// actor accepts is covered by the `ActorTask` unit tests.
class GlicActorConversationAnchorBrowserTest
    : public GlicActorFunctionalBrowserTestBase {
 public:
  GlicActorConversationAnchorBrowserTest()
      : GlicActorFunctionalBrowserTestBase(
            GlicTestJsPath("./glic_actor_conversation_anchor_browsertest.js")) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        critical_actions::features::kCriticalActionHistory,
        {{"enable_action_level_chat_linkouts", "true"}});
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&GlicActorConversationAnchorBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }
  ~GlicActorConversationAnchorBrowserTest() override = default;

  void SetUpOnMainThread() override {
    GlicActorFunctionalBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  const std::vector<std::string>& GetAnchorUpdates() const {
    auto* service = static_cast<AnchorRecordingCriticalActionService*>(
        critical_actions::CriticalActionFactory::GetForProfile(GetProfile()));
    CHECK(service);
    return service->anchor_updates();
  }

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    critical_actions::CriticalActionFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<AnchorRecordingCriticalActionService>(
              context->GetPath().AppendASCII("TestCriticalActions"));
        }));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(GlicActorConversationAnchorBrowserTest,
                       testTurnIdSetsConversationAnchor) {
  ExecuteJsTest();
  const std::string task_id =
      base::NumberToString(ExtractTaskIdFromStepData().value());

  // Each accepted call replaces the anchor: with its turn ID, or with no anchor
  // if it has none.
  EXPECT_THAT(GetAnchorUpdates(),
              ElementsAre(base::StrCat({"set:", task_id, ":r_turn_1"}),
                          base::StrCat({"set:", task_id, ":"})));
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicActorConversationAnchorBrowserTest,
                       testRejectedCallDoesNotSetConversationAnchor) {
  ExecuteJsTest();
  const std::string task_id =
      base::NumberToString(ExtractTaskIdFromStepData().value());

  // The call made while the task was paused was rejected, so only the call
  // made after resuming sets the anchor.
  EXPECT_THAT(GetAnchorUpdates(),
              ElementsAre(base::StrCat({"set:", task_id, ":r_resumed"})));
  ContinueJsTest();
}

}  // namespace
}  // namespace glic::actor
