// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"

#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_desktop_android.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/ui/side_panel/android/android_side_panel_enabled_fn.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::_;
using ::testing::Eq;

namespace glic {

class GlicSidePanelCoordinatorAndroidBrowserTest : public GlicBrowserTest {
 public:
  GlicSidePanelCoordinatorAndroidBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlicActor,
         {{features::kGlicActorPolicyControlExemption.name, "true"}}},
    };
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }
  ~GlicSidePanelCoordinatorAndroidBrowserTest() override = default;

  void SetUpOnMainThread() override {
    if (!AndroidSidePanelEnabledFn::IsEnabled()) {
      GTEST_SKIP() << "Android Side Panel is disabled";
    }
    if (!base::FeatureList::IsEnabled(features::kGlicAndroidSidePanel)) {
      GTEST_SKIP() << "Glic Android Side Panel is disabled";
    }
    GlicBrowserTest::SetUpOnMainThread();
    messages::MessageDispatcherBridge::SetInstanceForTesting(
        &mock_message_dispatcher_bridge_);
  }

  void TearDownOnMainThread() override {
    GlicBrowserTest::TearDownOnMainThread();
    messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  }

  GlicSidePanelCoordinatorDesktopAndroid* coordinator() {
    auto* tab = GetTabListInterface()->GetActiveTab();
    auto* coordinator = GlicSidePanelCoordinator::GetForTab(tab);
    CHECK(coordinator);
    return static_cast<GlicSidePanelCoordinatorDesktopAndroid*>(coordinator);
  }

  SidePanelEntryObserver* GetCoordinatorAsObserver() {
    return static_cast<SidePanelEntryObserver*>(coordinator());
  }

 protected:
  messages::MockMessageDispatcherBridge mock_message_dispatcher_bridge_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorAndroidBrowserTest,
                       ResizeTriggersToastAndShowsIdleMessage) {
  // Open Glic for the active tab to ensure the side panel is initialized.
  ASSERT_OK(OpenGlicForActiveTab());

  SidePanelEntryObserver* observer = GetCoordinatorAsObserver();

  // Expect the GlicToast message to be enqueued on the
  // MockMessageDispatcherBridge. It should display the exact bold title and
  // description subtitle.
  EXPECT_CALL(
      mock_message_dispatcher_bridge_,
      EnqueueMessage(
          testing::AllOf(
              testing::ResultOf(
                  [](messages::MessageWrapper* m) { return m->GetTitle(); },
                  l10n_util::GetStringUTF16(IDS_GLIC_CHAT_HIDDEN_TITLE)),
              testing::ResultOf(
                  [](messages::MessageWrapper* m) {
                    return m->GetDescription();
                  },
                  l10n_util::GetStringUTF16(IDS_GLIC_CHAT_HIDDEN_DESCRIPTION))),
          _, _, _))
      .WillOnce(testing::Return(true));

  // Trigger a window resize side panel hide event.
  observer->OnEntryHiddenWithReason(coordinator()->GetEntryForTesting(),
                                    SidePanelEntryHideReason::kWindowResized);

  // Re-show the Glic entry to ensure it auto-dismisses the toast banner.
  EXPECT_CALL(mock_message_dispatcher_bridge_, DismissMessage(_, _));

  observer->OnEntryShown(coordinator()->GetEntryForTesting());
}

IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorAndroidBrowserTest,
                       ResizeTriggersToastAndShowsActuatingMessage) {
  // Open Glic for the active tab.
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());

  // Simulate that Glic is currently actuating a task.
  ASSERT_OK(CreateActorTask(instance));

  SidePanelEntryObserver* observer = GetCoordinatorAsObserver();

  // Expect GlicToast to show the exact bold title and description subtitle.
  EXPECT_CALL(
      mock_message_dispatcher_bridge_,
      EnqueueMessage(
          testing::AllOf(
              testing::ResultOf(
                  [](messages::MessageWrapper* m) { return m->GetTitle(); },
                  l10n_util::GetStringUTF16(IDS_GLIC_TASK_PAUSED_TITLE)),
              testing::ResultOf(
                  [](messages::MessageWrapper* m) {
                    return m->GetDescription();
                  },
                  l10n_util::GetStringUTF16(IDS_GLIC_TASK_PAUSED_DESCRIPTION))),
          _, _, _))
      .WillOnce(testing::Return(true));

  // Trigger window resize hide event.
  observer->OnEntryHiddenWithReason(coordinator()->GetEntryForTesting(),
                                    SidePanelEntryHideReason::kWindowResized);
}

}  // namespace glic
