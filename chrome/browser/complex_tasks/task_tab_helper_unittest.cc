// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/complex_tasks/task_tab_helper.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/sessions/content/navigation_task_id.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockTaskTabHelper : public tasks::TaskTabHelper {
 public:
  static void CreateForWebContents(content::WebContents* contents) {
    DCHECK(contents);
    if (!FromWebContents(contents))
      contents->SetUserData(UserDataKey(),
                            base::WrapUnique(new MockTaskTabHelper(contents)));
  }

  static MockTaskTabHelper* FromWebContents(content::WebContents* contents) {
    DCHECK(contents);
    return static_cast<MockTaskTabHelper*>(
        contents->GetUserData(UserDataKey()));
  }

  explicit MockTaskTabHelper(content::WebContents* web_contents)
      : tasks::TaskTabHelper(web_contents) {}

  friend class TaskTabHelperUnitTest;
};

class TaskTabHelperUnitTest : public ChromeRenderViewHostTestHarness {
 protected:
  const std::string kSearchDomain = "http://www.google.com/";
  const GURL kSearchURL = GURL(kSearchDomain);

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    MockTaskTabHelper::CreateForWebContents(web_contents());
    task_tab_helper_ = MockTaskTabHelper::FromWebContents(web_contents());
    NavigateAndCommit(kSearchURL);
  }

  void GoBack() { content::NavigationSimulator::GoBack(web_contents()); }

  void GoBackNTimes(int times) {
    while (times--) {
      GoBack();
    }
  }

  void NavigateAndCommitNTimes(int times) {
    while (times--) {
      static int unique_int = 0;
      // Note: The URLs need to be different on each iteration. Otherwise,
      // navigations will be treated as reloads and will not create a new
      // NavigationEntry.
      NavigateAndCommit(
          GURL(kSearchDomain + base::NumberToString(++unique_int)));
    }
  }

  content::NavigationEntry* GetLastCommittedEntry() {
    return web_contents()->GetController().GetLastCommittedEntry();
  }

  raw_ptr<MockTaskTabHelper, DanglingUntriaged> task_tab_helper_;
};

TEST_F(TaskTabHelperUnitTest, TestGetCurrentTaskId) {
  std::unique_ptr<content::WebContents> test_parent_web_contents(
      content::WebContentsTester::CreateTestWebContents(
          web_contents()->GetBrowserContext(), nullptr));
  GURL parent_gurl("http://parent.com");
  content::WebContentsTester::For(test_parent_web_contents.get())
      ->NavigateAndCommit(parent_gurl);

  content::NavigationEntry* navigation_entry =
      test_parent_web_contents->GetController().GetLastCommittedEntry();
  sessions::NavigationTaskId* navigation_task_id =
      sessions::NavigationTaskId::Get(navigation_entry);
  navigation_task_id->set_id(3);
  navigation_task_id->set_root_id(4);
  EXPECT_EQ(
      tasks::TaskTabHelper::GetCurrentTaskId(test_parent_web_contents.get())
          ->id(),
      3);
  EXPECT_EQ(
      tasks::TaskTabHelper::GetCurrentTaskId(test_parent_web_contents.get())
          ->root_id(),
      4);
}

TEST_F(TaskTabHelperUnitTest, TestTaskIdExistingChain) {
  NavigateAndCommit(GURL("http://a.com"));
  sessions::NavigationTaskId a_navigation_task_id =
      *sessions::NavigationTaskId::Get(GetLastCommittedEntry());
  NavigateAndCommit(GURL("http://b.com"));
  sessions::NavigationTaskId b_navigation_task_id =
      *sessions::NavigationTaskId::Get(GetLastCommittedEntry());

  EXPECT_EQ(b_navigation_task_id.parent_id(), a_navigation_task_id.id());
  EXPECT_EQ(a_navigation_task_id.root_id(), b_navigation_task_id.root_id());
}

TEST_F(TaskTabHelperUnitTest, TestTaskIdNewChain) {
  NavigateAndCommit(GURL("http://a.com"));
  sessions::NavigationTaskId a_navigation_task_id =
      *sessions::NavigationTaskId::Get(GetLastCommittedEntry());

  NavigateAndCommit(GURL("http://b.com"), ui::PAGE_TRANSITION_TYPED);
  sessions::NavigationTaskId b_navigation_task_id =
      *sessions::NavigationTaskId::Get(GetLastCommittedEntry());

  EXPECT_EQ(b_navigation_task_id.parent_id(), -1);
  EXPECT_NE(a_navigation_task_id.root_id(), b_navigation_task_id.root_id());
}

TEST_F(TaskTabHelperUnitTest, TestTaskIdBackButton) {
  NavigateAndCommit(GURL("http://a.com"), ui::PAGE_TRANSITION_TYPED);
  NavigateAndCommit(GURL("http://b.com"));
  sessions::NavigationTaskId b_navigation_task_id =
      *sessions::NavigationTaskId::Get(GetLastCommittedEntry());
  GoBack();
  sessions::NavigationTaskId a_navigation_task_id =
      *sessions::NavigationTaskId::Get(GetLastCommittedEntry());

  // A should still have no parent after a back navigation and
  // shouldn't link to B (like it would if we navigated a.com -> b.com -> a.com
  // via clicking links)
  EXPECT_EQ(a_navigation_task_id.parent_id(), -1);
  EXPECT_NE(a_navigation_task_id.parent_id(), b_navigation_task_id.id());
}

TEST_F(TaskTabHelperUnitTest, TestTaskIdBackViaLink) {
  NavigateAndCommit(GURL("http://a.com"), ui::PAGE_TRANSITION_TYPED);
  NavigateAndCommit(GURL("http://b.com"));
  sessions::NavigationTaskId b_navigation_task_id =
      *sessions::NavigationTaskId::Get(GetLastCommittedEntry());
  NavigateAndCommit(GURL("http://a.com"));
  sessions::NavigationTaskId a_navigation_task_id =
      *sessions::NavigationTaskId::Get(GetLastCommittedEntry());

  // We got back to a.com via a link (not back button) so it should now point to
  // B.
  EXPECT_NE(a_navigation_task_id.parent_id(), -1);
  EXPECT_EQ(a_navigation_task_id.parent_id(), b_navigation_task_id.id());
}
