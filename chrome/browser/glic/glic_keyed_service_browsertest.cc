// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_prefs.h"
#include "chrome/browser/actor/ui/mock_event_dispatcher.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// The maximum number of times the closing toast should be shown for a profile.
constexpr int kToastShownMax = 2;

class GlicKeyedServiceBrowserTest : public InProcessBrowserTest {
 public:
  GlicKeyedServiceBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        // Both these features are required for the glic service to be properly
        // created.
        {{features::kGlic, {}},
         {features::kTabstripComboButton, {}},
         {features::kGlicActorUi, {{features::kGlicActorUiToastName, "true"}}}},
        /*disabled_features*/ {});
  }

  // Accessors
  PrefService* prefs() { return browser()->profile()->GetPrefs(); }

  // Helper Methods
  void CreateActingTask() {
    auto* actor_service = actor::ActorKeyedService::Get(browser()->profile());
    std::unique_ptr<actor::ExecutionEngine> execution_engine =
        std::make_unique<actor::ExecutionEngine>(browser()->profile());

    std::unique_ptr<actor::ActorTask> actor_task =
        std::make_unique<actor::ActorTask>(
            browser()->profile(), std::move(execution_engine),
            actor::ui::NewUiEventDispatcher(
                actor_service->GetActorUiStateManager()));
    actor_task->SetState(actor::ActorTask::State::kActing);

    actor_service->AddActiveTask(std::move(actor_task));
  }

  void CloseFloaty() {
    auto* glic_service = glic::GlicKeyedServiceFactory::GetGlicKeyedService(
        browser()->profile());

    // First open it so close pathway triggers.
    glic_service->window_controller().ShowDetachedForTesting();
    glic_service->ClosePanel();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicKeyedServiceBrowserTest, CallClosePanel_ExpectShow) {
  CreateActingTask();
  EXPECT_EQ(prefs()->GetInteger(actor::ui::kToastShown), 0);
  CloseFloaty();
  EXPECT_EQ(prefs()->GetInteger(actor::ui::kToastShown), 1);
}

IN_PROC_BROWSER_TEST_F(GlicKeyedServiceBrowserTest,
                       CallClosePanel_ExpectShowsMaxTimes) {
  CreateActingTask();

  // Close the panel kToastShownMax times.
  for (int i = 0; i < kToastShownMax; i++) {
    CloseFloaty();
    EXPECT_EQ(prefs()->GetInteger(actor::ui::kToastShown), i + 1);
  }

  // Close it one more time. Ensure pref did not update.
  CloseFloaty();
  EXPECT_EQ(prefs()->GetInteger(actor::ui::kToastShown), kToastShownMax);
}

}  // namespace
