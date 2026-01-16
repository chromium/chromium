// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/glic_tab_observer_android.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/glic/common/glic_tab_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;

class GlicTabObserverAndroidTest : public ChromeRenderViewHostTestHarness {
 public:
  GlicTabObserverAndroidTest() = default;
};

TEST_F(GlicTabObserverAndroidTest, TabAdditionNotifiesObserver) {
  base::MockCallback<GlicTabObserver::EventCallback> mock_callback;
  OwningTestTabModel tab_model(profile());
  tab_model.AddEmptyTab(0, /*select=*/true,
                        TabModel::TabLaunchType::FROM_CHROME_UI);
  GlicTabObserverAndroid observer(profile(), mock_callback.Get());

  EXPECT_CALL(mock_callback, Run(_)).WillOnce([](const GlicTabEvent& event) {
    ASSERT_TRUE(std::holds_alternative<TabCreationEvent>(event));
    const auto& creation_event = std::get<TabCreationEvent>(event);
    EXPECT_NE(nullptr, creation_event.new_tab);
    EXPECT_EQ(nullptr, creation_event.old_tab);
    EXPECT_EQ(TabCreationType::kUserInitiated, creation_event.creation_type);
  });

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  tab_model.AddTabFromWebContents(
      std::move(web_contents), 0, false,
      TabModel::TabLaunchType::FROM_RECENT_TABS_FOREGROUND);
  TabModelList::RemoveObserver(&observer);
}

TEST_F(GlicTabObserverAndroidTest, TabSelectionNotifiesObserver) {
  base::MockCallback<GlicTabObserver::EventCallback> mock_callback;
  OwningTestTabModel tab_model(profile());
  GlicTabObserverAndroid observer(profile(), mock_callback.Get());

  // Add two tabs.
  std::unique_ptr<content::WebContents> web_contents1 =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  TabAndroid* tab1 =
      tab_model.AddTabFromWebContents(std::move(web_contents1), 0, true,
                                      TabModel::TabLaunchType::FROM_CHROME_UI);

  std::unique_ptr<content::WebContents> web_contents2 =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  tab_model.AddTabFromWebContents(std::move(web_contents2), 1, false,
                                  TabModel::TabLaunchType::FROM_CHROME_UI);

  EXPECT_CALL(mock_callback, Run(_)).WillOnce([&](const GlicTabEvent& event) {
    ASSERT_TRUE(std::holds_alternative<TabActivationEvent>(event));
    const auto& activation_event = std::get<TabActivationEvent>(event);
    EXPECT_NE(nullptr, activation_event.new_active_tab);
    EXPECT_EQ(tab1, activation_event.old_active_tab);
  });

  tab_model.SetActiveIndex(1);
  TabModelList::RemoveObserver(&observer);
}

TEST_F(GlicTabObserverAndroidTest, TabRemovalNotifiesObserver) {
  base::MockCallback<GlicTabObserver::EventCallback> mock_callback;
  OwningTestTabModel tab_model(profile());
  GlicTabObserverAndroid observer(profile(), mock_callback.Get());

  // Add a tab.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  tab_model.AddTabFromWebContents(std::move(web_contents), 0, true,
                                  TabModel::TabLaunchType::FROM_CHROME_UI);

  EXPECT_CALL(mock_callback, Run(_)).WillOnce([](const GlicTabEvent& event) {
    ASSERT_TRUE(std::holds_alternative<TabMutationEvent>(event));
  });

  tab_model.CloseTabAt(0);
  TabModelList::RemoveObserver(&observer);
}
