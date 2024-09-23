// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/user_session/arc_user_session_service.h"

#include <memory>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_test.h"

namespace arc {

namespace {

constexpr char kUserSessionActiveBroadcastAction[] =
    "org.chromium.arc.intent_helper.USER_SESSION_ACTIVE";

// Returns the number of |broadcasts| having the USER_SESSION_ACTIVE action.
int CountBroadcasts(
    const std::vector<FakeIntentHelperInstance::Broadcast>& broadcasts) {
  int count = 0;
  for (const FakeIntentHelperInstance::Broadcast& broadcast : broadcasts) {
    if (broadcast.action == kUserSessionActiveBroadcastAction) {
      count++;
    }
  }
  return count;
}

void RunUntilIdle() {
  DCHECK(base::CurrentThread::Get());
  base::RunLoop().RunUntilIdle();
}

}  // namespace

class ArcUserSessionServiceTest : public InProcessBrowserTest {
 public:
  ArcUserSessionServiceTest() = default;

  ArcUserSessionServiceTest(const ArcUserSessionServiceTest&) = delete;
  ArcUserSessionServiceTest& operator=(const ArcUserSessionServiceTest&) =
      delete;

  ~ArcUserSessionServiceTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    RunUntilIdle();

    fake_intent_helper_instance_ = std::make_unique<FakeIntentHelperInstance>();
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->intent_helper()
        ->SetInstance(fake_intent_helper_instance_.get());
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->intent_helper());
  }

  void TearDownOnMainThread() override {
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->intent_helper()
        ->CloseInstance(fake_intent_helper_instance_.get());
    fake_intent_helper_instance_.reset(nullptr);
  }

 protected:
  std::unique_ptr<FakeIntentHelperInstance> fake_intent_helper_instance_;
};

IN_PROC_BROWSER_TEST_F(ArcUserSessionServiceTest,
                       FiresIntentOnSessionActiveTest) {
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();

  // Simulate locking, then unlocking, the session.
  session_manager->SetSessionState(session_manager::SessionState::LOCKED);
  session_manager->SetSessionState(session_manager::SessionState::ACTIVE);

  // The broadcast should have been sent once.
  EXPECT_EQ(1, CountBroadcasts(fake_intent_helper_instance_->broadcasts()));
}

}  // namespace arc
