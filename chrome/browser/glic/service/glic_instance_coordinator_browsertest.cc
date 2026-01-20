// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: Avoid using RunTestSequence unless absolutely necessary. Simple
// synchronous operations should be called directly to keep tests easy to read
// and debug. When waiting is required, `WaitUntil` is usually sufficient and
// simpler than a full `RunTestSequence`.

#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_close_options.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace glic {

class GlicInstanceCoordinatorBrowserTest : public NonInteractiveGlicTest {
 public:
  GlicInstanceCoordinatorBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicMultiInstance,
                              features::kGlicDaisyChainNewTabs,
                              features::kGlicWebContentsWarming},
        /*disabled_features=*/{});
  }
  ~GlicInstanceCoordinatorBrowserTest() override = default;

  GlicInstanceCoordinatorImpl& coordinator() {
    return static_cast<GlicInstanceCoordinatorImpl&>(window_controller());
  }

  void ToggleGlic(bool prevent_close = false) {
    coordinator().Toggle(browser(), prevent_close,
                         mojom::InvocationSource::kOsButton,
                         /*prompt_suggestion=*/std::nullopt);
  }

  tabs::TabInterface* AddTab() {
    content::WebContents* contents = chrome::AddAndReturnTabAt(
        browser(), GURL("about:blank"), /*tab_index=*/-1, /*foreground=*/true);
    return tabs::TabInterface::GetFromContents(contents);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest, CoordinatorExists) {
  EXPECT_TRUE(&coordinator());
  EXPECT_EQ(&coordinator(),
            &GlicKeyedService::Get(browser()->profile())->window_controller());
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest, InitialState) {
  EXPECT_EQ(coordinator().GetInstances().size(), 0u);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       ToggleCreatesInstance) {
  ToggleGlic();
  EXPECT_EQ(coordinator().GetInstances().size(), 1u);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest, CloseHidesInstance) {
  RunTestSequence(Do([this]() { ToggleGlic(); }), WaitForGlicOpen(),
                  Do([this]() { ToggleGlic(); }), WaitForGlicClose());
  for (auto* instance : coordinator().GetInstances()) {
    EXPECT_FALSE(instance->IsShowing());
  }
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       CreateConversationForTabs) {
  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();
  tabs::TabInterface* tab2 = AddTab();

  coordinator().CreateNewConversationForTabs({tab1, tab2});

  EXPECT_TRUE(coordinator().GetInstanceForTab(tab1));
  EXPECT_EQ(coordinator().GetInstanceForTab(tab1),
            coordinator().GetInstanceForTab(tab2));
  EXPECT_TRUE(coordinator().GetInstanceForTab(tab1)->IsShowing());
  EXPECT_FALSE(coordinator().GetInstances().empty());
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       NewTabDaisyChaining) {
  browser()->profile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, true);

  ToggleGlic();

  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();
  tabs::TabInterface* tab2 = AddTab();

  EXPECT_TRUE(coordinator().GetInstanceForTab(tab1));
  EXPECT_TRUE(coordinator().GetInstanceForTab(tab2));
  EXPECT_NE(coordinator().GetInstanceForTab(tab1),
            coordinator().GetInstanceForTab(tab2));
  EXPECT_TRUE(coordinator().GetInstanceForTab(tab2)->IsShowing());

  browser()->profile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, false);
  tabs::TabInterface* tab3 = AddTab();
  EXPECT_FALSE(coordinator().GetInstanceForTab(tab3));
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorBrowserTest,
                       MoveTabsToConversation) {
  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();
  ToggleGlic();
  tabs::TabInterface* tab2 = AddTab();
  ToggleGlic();
  ASSERT_TRUE(tab1);
  ASSERT_TRUE(tab2);

  GlicInstanceImpl* instance1 = coordinator().GetInstanceImplForTab(tab1);
  EXPECT_TRUE(instance1);
  GlicInstanceImpl* instance2 = coordinator().GetInstanceImplForTab(tab2);
  EXPECT_TRUE(instance2);
  EXPECT_NE(instance1, instance2);

  // Assign a conversation ID to instance2 so it can be targeted.
  // In production, this comes from the web client.
  const std::string kTargetConversationId = "conv_2";
  auto info = glic::mojom::ConversationInfo::New();
  info->conversation_id = kTargetConversationId;
  instance2->RegisterConversation(std::move(info), base::DoNothing());

  // Move tab1 to instance2's conversation.
  coordinator().ShowInstanceForTabs({tab1}, instance2->id());

  EXPECT_EQ(coordinator().GetInstanceForTab(tab1), instance2);
  EXPECT_EQ(coordinator().GetInstanceForTab(tab2), instance2);
}

}  // namespace glic
