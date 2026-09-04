// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/app_session_service_test_helper.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_load_waiter.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_test_path_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/collaboration/public/messaging/empty_messaging_backend_service.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sessions/core/command_storage_backend.h"
#include "components/sessions/core/command_storage_features.h"
#include "components/sessions/core/command_storage_manager.h"
#include "components/sessions/core/command_storage_manager_test_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_impl.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#endif

namespace sessions {

using SessionType = sessions::CommandStorageManager::SessionType;
using internal::kEncryptSessionStorageStageWriteBothReadOnlyClear;
using internal::kEncryptSessionStorageStageWriteBothReadPreferEncrypted;
using internal::kEncryptSessionStorageStageWriteEncryptedReadPreferEncrypted;

struct TestParams {
  bool encryption_enabled;    // Enables feature kEncryptSessionStorage.
  const char* rollout_stage;  // Feature param kEncryptSessionStorage::stage.
};

std::string TestParamNameGenerator(
    const testing::TestParamInfo<TestParams>& info) {
  return info.param.encryption_enabled ? info.param.rollout_stage
                                       : "clear_only";
}

// Base fixture for command storage encryption browser tests.
// Provides common utilities (adding tabs, quitting and restoring the browser,
// tab restore operations).
class EncryptedSessionStorageBrowserTestBase : public InProcessBrowserTest {
 public:
  EncryptedSessionStorageBrowserTestBase() {
    dependency_manager_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &EncryptedSessionStorageBrowserTestBase::RegisterFakeServices,
                base::Unretained(this)));
  }

 protected:
  void InitEncryptionFeature(const TestParams& params) {
    if (!params.encryption_enabled) {
      scoped_feature_list_.InitAndDisableFeature(kEncryptSessionStorage);
    } else {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          kEncryptSessionStorage, {{"stage", params.rollout_stage}});
    }
  }

  void VerifyWindowBounds(gfx::Rect expected, gfx::Rect actual) {
    bool is_wayland = false;
#if BUILDFLAG(IS_OZONE)
    is_wayland = ::ui::OzonePlatform::RunningOnWaylandForTest();
#endif
    if (is_wayland) {
      // On Linux Wayland, the client cannot set top-level window positions.
      EXPECT_EQ(expected.size(), actual.size());
    } else {
      EXPECT_EQ(expected, actual);
    }
  }

  void AssertCommandStorageBackendFilesExist(SessionType session_type,
                                             Profile* profile = nullptr) {
    if (!profile) {
      profile = browser()->GetProfile();
    }
    base::ScopedAllowBlockingForTesting allow_blocking;
    sessions::CommandStorageManager* command_storage_manager = nullptr;
    switch (session_type) {
      case SessionType::kSessionRestore: {
        SessionServiceTestHelper helper(
            SessionServiceFactory::GetForProfile(profile));
        command_storage_manager = helper.command_storage_manager();
        break;
      }
      case SessionType::kTabRestore: {
        sessions::TabRestoreService* service =
            TabRestoreServiceFactory::GetForProfile(profile);
        if (service) {
          sessions::TabRestoreServiceImpl* helper =
              static_cast<sessions::TabRestoreServiceImpl*>(service);
          command_storage_manager =
              helper->command_storage_manager_for_testing();
        }
        break;
      }
      case SessionType::kAppRestore:
        // Not yet needed for testing.
        break;
    }
    CHECK(command_storage_manager);
    sessions::CommandStorageManagerTestHelper test_helper(
        command_storage_manager);
    command_storage_manager->Save();
    test_helper.RunMessageLoopUntilBackendDone();
    sessions::CommandStorageBackend* cleartext_backend =
        test_helper.GetCleartextBackend();
    sessions::CommandStorageBackend* encrypted_backend =
        test_helper.GetEncryptedBackend();
    if (test_helper.ShouldWriteEncryptedFiles()) {
      // The encrypted backend isn't initialized unless it's used.
      ASSERT_TRUE(encrypted_backend);
      ASSERT_TRUE(
          base::PathExists(encrypted_backend->current_path_for_testing()));
    } else {
      // When it's not initialized, the related directory should be deleted.
      ASSERT_FALSE(encrypted_backend);
      ASSERT_FALSE(base::PathExists(
          command_storage_manager->GetBackendDirectoryForTesting(
              /*is_encrypted=*/true)));
    }
    // The cleartext backend is always created.
    ASSERT_TRUE(cleartext_backend);
    if (test_helper.ShouldWriteCleartextFiles()) {
      // If it was used it should exist.
      ASSERT_TRUE(
          base::PathExists(cleartext_backend->current_path_for_testing()));
    } else {
      // There's a special case for tests that go from phase 0 to 3, as they do
      // end up keeping the cleartext files until the next relaunch. This is due
      // to needing the data for the initial launch as a source of truth.
      if (base::EndsWith(
              testing::UnitTest::GetInstance()->current_test_info()->name(),
              "clear_only_To_write_encrypted_read_prefer_encrypted")) {
        return;
      }
      // Otherwise, it may or may not exist, but either way it should be empty.
      ASSERT_TRUE(base::IsDirectoryEmpty(
          command_storage_manager->GetBackendDirectoryForTesting(
              /*is_encrypted=*/false)));
    }
  }

 protected:
  // Registers fake implementations of services before they are created. This
  // prevents complex collaboration and messaging database/sync services from
  // initializing, avoiding unnecessary dependencies during session tests.
  void RegisterFakeServices(content::BrowserContext* context) {
    collaboration::messaging::MessagingBackendServiceFactory::GetInstance()
        ->SetTestingFactory(
            context, base::BindRepeating([](content::BrowserContext* context)
                                             -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  collaboration::messaging::EmptyMessagingBackendService>();
            }));
  }
#if BUILDFLAG(IS_CHROMEOS)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kCreateBrowserOnStartupForTests);
  }
#endif

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());

#if BUILDFLAG(IS_CHROMEOS)
    SessionServiceTestHelper helper(browser()->GetProfile());
    helper.SetForceBrowserNotAliveWithNoWindows(true);
#endif

    if (browser()) {
      SessionStartupPref pref(SessionStartupPref::LAST);
      SessionStartupPref::SetStartupPref(browser()->GetProfile(), pref);
    }
  }

  GURL GetUrl(int index) {
    CHECK_GE(index, 1);
    // Only bot1.html, bot2.html, and bot3.html exist in session_history.
    // Cycle through existing files rather than navigating to non-existent
    // files that trigger error pages.
    const int file_index = ((index - 1) % 3) + 1;
    return chrome_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("session_history"),
        base::FilePath().AppendASCII("bot" + base::NumberToString(file_index) +
                                     ".html"));
  }

  // Synchronously closes the specified browser and launches a new window
  // to trigger session restoration. Optionally navigates the new window to
  // `url` and waits for all restored tabs to finish loading if
  // `no_memory_pressure` is true.
  BrowserWindowInterface* QuitBrowserAndRestore(
      BrowserWindowInterface* browser,
      const GURL& url = GURL(),
      bool no_memory_pressure = true) {
    Profile* profile = browser->GetProfile();

    auto keep_alive = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
    auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
        profile, ProfileKeepAliveOrigin::kBrowserWindow);
    CloseBrowserSynchronously(browser);

    ui_test_utils::AllBrowserTabAddedWaiter tab_waiter;
    SessionRestoreTestHelper restore_observer;

    SessionServiceTestHelper helper(profile);
    helper.SetForceBrowserNotAliveWithNoWindows(true);

    profile->GetDefaultStoragePartition()
        ->OverrideDeleteStaleSessionCleanupDelayForTesting(base::Minutes(0));

    if (url.is_empty()) {
      chrome::NewEmptyWindow(profile);
    } else {
      NavigateParams params(profile, url, ui::PAGE_TRANSITION_LINK);
      Navigate(&params);
    }

    content::WebContents* contents = tab_waiter.Wait();
    BrowserWindowInterface* new_browser = nullptr;
    for (auto* window : GetAllBrowserWindowInterfaces()) {
      if (window->GetTabStripModel()->GetIndexOfWebContents(contents) !=
          TabStripModel::kNoTab) {
        new_browser = window;
        break;
      }
    }

    restore_observer.Wait();

    if (no_memory_pressure) {
      EnsureTabsLoaded(new_browser->GetTabStripModel());
    }

    keep_alive.reset();
    profile_keep_alive.reset();

    return new_browser;
  }

  void CloseTab(int index) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(index);
    content::RenderFrameDeletedObserver deleted_observer(
        web_contents->GetPrimaryMainFrame());
    browser()->tab_strip_model()->CloseWebContentsAt(
        index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
    deleted_observer.WaitUntilDeleted();
  }

  void RestoreTab(BrowserWindowInterface* target_browser,
                  int expected_tabstrip_index) {
    ui_test_utils::AllBrowserTabAddedWaiter tab_added_waiter;
    {
      sessions::TabRestoreService* service =
          TabRestoreServiceFactory::GetForProfile(target_browser->GetProfile());
      TabRestoreServiceLoadWaiter waiter(service);
      waiter.Wait();
      SessionID target_id = SessionID::InvalidValue();
      for (const auto& entry : service->entries()) {
        if (entry->type == sessions::tab_restore::Type::TAB) {
          target_id = entry->id;
          break;
        }
      }
      if (target_id.is_valid()) {
        service->RestoreEntryById(BrowserLiveTabContext::From(target_browser),
                                  target_id,
                                  WindowOpenDisposition::NEW_FOREGROUND_TAB);
      } else {
        chrome::RestoreTab(target_browser);
      }
    }
    content::WebContents* new_tab = tab_added_waiter.Wait();
    content::WaitForLoadStop(new_tab);
    EXPECT_EQ(expected_tabstrip_index,
              target_browser->GetTabStripModel()->active_index());
  }

  void AddFileSchemeTabs(BrowserWindowInterface* browser,
                         int how_many,
                         int starting_url_index = 1) {
    int starting_tab_count = browser->GetTabStripModel()->count();
    for (int i = 0; i < how_many; ++i) {
      ui_test_utils::NavigateToURLWithDisposition(
          browser, GetUrl(starting_url_index + i),
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    }
    int tab_count = browser->GetTabStripModel()->count();
    EXPECT_EQ(starting_tab_count + how_many, tab_count);
  }

  void EnsureTabsLoaded(TabStripModel* tab_strip_model) {
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
      contents->GetController().LoadIfNecessary();
      content::WaitForLoadStop(contents);
    }
  }

  void CloseAboutBlankTabs(BrowserWindowInterface* browser) {
    TabStripModel* tab_strip_model = browser->GetTabStripModel();
    for (int i = tab_strip_model->count() - 1; i >= 0; --i) {
      if (tab_strip_model->count() <= 1) {
        return;
      }
      if (tab_strip_model->GetWebContentsAt(i)->GetURL().IsAboutBlank()) {
        content::RenderFrameDeletedObserver deleted_observer(
            tab_strip_model->GetWebContentsAt(i)->GetPrimaryMainFrame());
        tab_strip_model->CloseWebContentsAt(i, TabCloseTypes::CLOSE_NONE);
        deleted_observer.WaitUntilDeleted();
      }
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription dependency_manager_subscription_;
};

// Tests Session Restore functionality for a particular stage of the command
// storage encryption rollout. Its purpose is to verify that sessions are
// written and restored correctly.  For tests that span multiple
// stages of rollout, see the SessionRestoreAcrossStagesTest.
class SessionRestoreWithEncryptionTest
    : public EncryptedSessionStorageBrowserTestBase,
      public testing::WithParamInterface<TestParams> {
 public:
  void SetUp() override {
    InitEncryptionFeature(GetParam());
    EncryptedSessionStorageBrowserTestBase::SetUp();
  }
};

IN_PROC_BROWSER_TEST_P(SessionRestoreWithEncryptionTest, BasicRestore) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(1)));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl(2), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  AssertCommandStorageBackendFilesExist(SessionType::kSessionRestore);

  BrowserWindowInterface* restored = QuitBrowserAndRestore(browser());
  TabStripModel* tab_strip_model = restored->GetTabStripModel();
  ASSERT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(GetUrl(1), tab_strip_model->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(GetUrl(2), tab_strip_model->GetWebContentsAt(1)->GetURL());
}

IN_PROC_BROWSER_TEST_P(SessionRestoreWithEncryptionTest, LargeSessionRestore) {
  constexpr int kNumTabs = 20;
  AddFileSchemeTabs(browser(), kNumTabs);
  int starting_tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(kNumTabs + 1, starting_tab_count);
  AssertCommandStorageBackendFilesExist(SessionType::kSessionRestore);

  BrowserWindowInterface* restored = QuitBrowserAndRestore(browser());
  TabStripModel* tab_strip_model = restored->GetTabStripModel();
  EXPECT_EQ(starting_tab_count, tab_strip_model->count());
  for (int i = 1; i < starting_tab_count; ++i) {
    EXPECT_EQ(GetUrl(i), tab_strip_model->GetWebContentsAt(i)->GetURL());
  }
}

IN_PROC_BROWSER_TEST_P(SessionRestoreWithEncryptionTest,
                       NavigationHistoryIsRestored) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(1)));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl(2), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // Navigate the active tab (index 1) to GetUrl(3), keeping GetUrl(2) in
  // history.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(3)));

  AssertCommandStorageBackendFilesExist(SessionType::kSessionRestore);

  BrowserWindowInterface* restored = QuitBrowserAndRestore(browser());
  TabStripModel* tab_strip_model = restored->GetTabStripModel();
  ASSERT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(GetUrl(1), tab_strip_model->GetWebContentsAt(0)->GetURL());

  content::WebContents* restored_tab2 = tab_strip_model->GetWebContentsAt(1);
  EXPECT_EQ(GetUrl(3), restored_tab2->GetURL());

  content::NavigationController& controller = restored_tab2->GetController();
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(GetUrl(2), controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(GetUrl(3), controller.GetEntryAtIndex(1)->GetURL());
}

IN_PROC_BROWSER_TEST_P(SessionRestoreWithEncryptionTest, TabGroupsAreRestored) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(1)));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl(2), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl(3), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});
  browser()->tab_strip_model()->ChangeTabGroupVisuals(
      group_id, tab_groups::TabGroupVisualData(
                    u"Work", tab_groups::TabGroupColorId::kBlue));

  AssertCommandStorageBackendFilesExist(SessionType::kSessionRestore);

  BrowserWindowInterface* restored = QuitBrowserAndRestore(browser());
  TabStripModel* tab_strip_model = restored->GetTabStripModel();
  ASSERT_EQ(3, tab_strip_model->count());

  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(0).has_value());

  std::optional<tab_groups::TabGroupId> group_id1 =
      tab_strip_model->GetTabGroupForTab(1);
  std::optional<tab_groups::TabGroupId> group_id2 =
      tab_strip_model->GetTabGroupForTab(2);
  ASSERT_TRUE(group_id1.has_value());
  ASSERT_TRUE(group_id2.has_value());
  EXPECT_EQ(group_id1.value(), group_id2.value());

  const tab_groups::TabGroupVisualData* visual_data =
      tab_strip_model->group_model()
          ->GetTabGroup(group_id1.value())
          ->visual_data();
  ASSERT_TRUE(visual_data);
  EXPECT_EQ(u"Work", visual_data->title());
  EXPECT_EQ(tab_groups::TabGroupColorId::kBlue, visual_data->color());
}

IN_PROC_BROWSER_TEST_P(SessionRestoreWithEncryptionTest,
                       PinnedTabsAreRestored) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(1)));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl(2), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  browser()->tab_strip_model()->SetTabPinned(0, true);

  AssertCommandStorageBackendFilesExist(SessionType::kSessionRestore);

  BrowserWindowInterface* restored = QuitBrowserAndRestore(browser());
  TabStripModel* tab_strip_model = restored->GetTabStripModel();
  ASSERT_EQ(2, tab_strip_model->count());
  EXPECT_TRUE(tab_strip_model->IsTabPinned(0));
  EXPECT_FALSE(tab_strip_model->IsTabPinned(1));
}

IN_PROC_BROWSER_TEST_P(SessionRestoreWithEncryptionTest, ActiveTabIsRestored) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(1)));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl(2), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  browser()->tab_strip_model()->ActivateTabAt(0);

  AssertCommandStorageBackendFilesExist(SessionType::kSessionRestore);

  BrowserWindowInterface* restored = QuitBrowserAndRestore(browser());
  TabStripModel* tab_strip_model = restored->GetTabStripModel();
  ASSERT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(0, tab_strip_model->active_index());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SessionRestoreWithEncryptionTest,
    testing::Values(
        TestParams{false, ""},
        TestParams{true, kEncryptSessionStorageStageWriteBothReadOnlyClear},
        TestParams{true,
                   kEncryptSessionStorageStageWriteBothReadPreferEncrypted},
        TestParams{
            true,
            kEncryptSessionStorageStageWriteEncryptedReadPreferEncrypted}),
    TestParamNameGenerator);

// Tests Tab Restore functionality for a particular stage of the command storage
// encryption rollout. Its purpose is to verify that single tabs and sets
// of tabs are written and restored cleanly from historical records.  For
// tests that span multiple stages of rollout, see the
// TabRestoreAcrossStagesTest.
class TabRestoreWithEncryptionTest
    : public EncryptedSessionStorageBrowserTestBase,
      public testing::WithParamInterface<TestParams> {
 public:
  void SetUp() override {
    InitEncryptionFeature(GetParam());
    EncryptedSessionStorageBrowserTestBase::SetUp();
  }
};

IN_PROC_BROWSER_TEST_P(TabRestoreWithEncryptionTest, BasicRestore) {
  AddFileSchemeTabs(browser(), 1);
  int starting_tab_count = browser()->tab_strip_model()->count();

  CloseTab(starting_tab_count - 1);
  EXPECT_EQ(starting_tab_count - 1, browser()->tab_strip_model()->count());
  AssertCommandStorageBackendFilesExist(SessionType::kTabRestore);

  ASSERT_NO_FATAL_FAILURE(RestoreTab(browser(), starting_tab_count - 1));

  EXPECT_EQ(starting_tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetUrl(1),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(TabRestoreWithEncryptionTest, LargeSessionRestore) {
  constexpr int kNumTabs = 20;
  AddFileSchemeTabs(browser(), kNumTabs);
  int starting_tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(kNumTabs + 1, starting_tab_count);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  EXPECT_EQ(2u, GlobalBrowserCollection::GetInstance()->GetSize());

  BrowserWindowInterface* browser1 = browser();
  CloseBrowserSynchronously(browser1);
  EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());

  BrowserWindowInterface* browser2 =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  AssertCommandStorageBackendFilesExist(SessionType::kTabRestore,
                                        browser2->GetProfile());

  ui_test_utils::BrowserCreatedObserver observer;
  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser2->GetProfile());
  service->RestoreMostRecentEntry(BrowserLiveTabContext::From(browser2));
  BrowserWindowInterface* restored_browser = observer.Wait();

  EXPECT_EQ(starting_tab_count, restored_browser->GetTabStripModel()->count());
  for (int i = 1; i < starting_tab_count; ++i) {
    EXPECT_EQ(
        GetUrl(i),
        restored_browser->GetTabStripModel()->GetWebContentsAt(i)->GetURL());
  }
}

IN_PROC_BROWSER_TEST_P(SessionRestoreWithEncryptionTest,
                       WindowBoundsAreRestored) {
  gfx::Rect expected_bounds(50, 50, 550, 500);
  browser()->GetWindow()->SetBounds(expected_bounds);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    // Wait for window to be updated.  Only check window size because some
    // Window Managers do not update position or adjust the position.
    return browser()->GetWindow()->GetBounds().size() == expected_bounds.size();
  }));
  // Capture the actual OS-granted bounds so we verify against what session
  // restore will save.
  expected_bounds = browser()->GetWindow()->GetBounds();

  // Navigate to trigger SessionService creation and record the new bounds.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(1)));

  AssertCommandStorageBackendFilesExist(SessionType::kSessionRestore);
  BrowserWindowInterface* restored = QuitBrowserAndRestore(browser());

  // Navigate to trigger SessionService creation and compare the bounds.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(restored, GetUrl(1)));
#if BUILDFLAG(IS_MAC)
  // On MacOS, relaunch behavior differs from other platforms so evaluating
  // window size restore is difficult. See https://crrev.com/c/8006276.
#else
  VerifyWindowBounds(expected_bounds, restored->GetWindow()->GetBounds());
#endif
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TabRestoreWithEncryptionTest,
    testing::Values(
        TestParams{false, ""},
        TestParams{true, kEncryptSessionStorageStageWriteBothReadOnlyClear},
        TestParams{true,
                   kEncryptSessionStorageStageWriteBothReadPreferEncrypted},
        TestParams{
            true,
            kEncryptSessionStorageStageWriteEncryptedReadPreferEncrypted}),
    TestParamNameGenerator);

struct StageTransitionTestParams {
  TestParams before;
  TestParams after;
};

std::string StageTransitionNameGenerator(
    const testing::TestParamInfo<StageTransitionTestParams>& info) {
  auto get_name = [](const TestParams& params) -> std::string {
    return params.encryption_enabled ? params.rollout_stage : "clear_only";
  };
  return get_name(info.param.before) + "_To_" + get_name(info.param.after);
}

class RestoreAcrossStagesTestBase
    : public EncryptedSessionStorageBrowserTestBase,
      public testing::WithParamInterface<StageTransitionTestParams> {
 public:
  std::vector<TestParams> GetTestParamsForEachStep() {
    return {GetParam().before, GetParam().after};
  }

  void SetUp() override {
    auto test_params = GetTestParamsForEachStep();
    CHECK(test_params.size() > GetTestPreCount())
        << "One or more test steps are missing test params.";
    TestParams params = test_params[test_params.size() - GetTestPreCount() - 1];
    InitEncryptionFeature(params);
    EncryptedSessionStorageBrowserTestBase::SetUp();
  }
};

// Tests session restore behavior across browser restarts, where the encryption
// rollout stage changes between restarts.
//
// Session restore includes: navigation history, tab groups, pinned tabs,
// the active tab index, and window state/bounds.
//
// Separate test TabRestoreTestAcrossStagesTest will verify that the correct
// tabs are restored.
class SessionRestoreAcrossStagesTest : public RestoreAcrossStagesTestBase {
 public:
  static constexpr gfx::Rect kWindowBounds1{50, 50, 550, 500};
  static constexpr gfx::Rect kWindowBounds2{200, 50, 550, 500};

  // Saves the bounds of the two windows to a JSON file.
  // This file is required because this test class runs across multiple restarts
  // of the browser (i.e. each PRE_ step is executed in a separate process).
  // Therefore, in-memory state like member variables is lost between restarts,
  // and we must persist the actual bounds on disk in the profile directory.
  void SaveWindowBoundsToFile(base::FilePath file_path,
                              BrowserWindowInterface* w1,
                              BrowserWindowInterface* w2) {
    base::DictValue dict;
    dict.Set("x1", w1->GetWindow()->GetBounds().x());
    dict.Set("y1", w1->GetWindow()->GetBounds().y());
    dict.Set("w1", w1->GetWindow()->GetBounds().width());
    dict.Set("h1", w1->GetWindow()->GetBounds().height());
    dict.Set("x2", w2->GetWindow()->GetBounds().x());
    dict.Set("y2", w2->GetWindow()->GetBounds().y());
    dict.Set("w2", w2->GetWindow()->GetBounds().width());
    dict.Set("h2", w2->GetWindow()->GetBounds().height());

    std::string json_output;
    base::JSONWriter::Write(dict, &json_output);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::WriteFile(file_path, json_output);
    }
  }

  // Reads the expected bounds of the two windows from the JSON file.
  std::pair<gfx::Rect, gfx::Rect> ReadWindowBoundsFromFile(
      base::FilePath file_path) {
    std::string json_input;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      if (!base::ReadFileToString(file_path, &json_input)) {
        ADD_FAILURE() << "Failed to read window bounds from " << file_path;
        return {gfx::Rect(), gfx::Rect()};
      }
    }
    std::optional<base::DictValue> dict_opt =
        base::JSONReader::ReadDict(json_input, base::JSON_PARSE_RFC);
    if (!dict_opt.has_value()) {
      return {gfx::Rect(), gfx::Rect()};
    }
    const base::DictValue& dict = dict_opt.value();
    gfx::Rect bounds1(
        dict.FindInt("x1").value_or(0), dict.FindInt("y1").value_or(0),
        dict.FindInt("w1").value_or(0), dict.FindInt("h1").value_or(0));
    gfx::Rect bounds2(
        dict.FindInt("x2").value_or(0), dict.FindInt("y2").value_or(0),
        dict.FindInt("w2").value_or(0), dict.FindInt("h2").value_or(0));
    return {bounds1, bounds2};
  }

  void SetUpSessionState() {
    // Window 1 is browser(). Put it on the left half of the screen.
    browser()->GetWindow()->SetBounds(kWindowBounds1);

    // Window 1 Tab 1 shows GetUrl(1)
    CloseAboutBlankTabs(browser());
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                              GetUrl(1), 1);

    // Window 1 Tab 2 shows GetUrl(3), with GetUrl(2) in history
    chrome::AddSelectedTabWithURL(browser(), GetUrl(2),
                                  ui::PAGE_TRANSITION_TYPED);
    content::WaitForLoadStop(
        browser()->tab_strip_model()->GetActiveWebContents());
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                              GetUrl(3), 1);

    // Ensure Window 1 Tab 1 is active (index 0)
    browser()->tab_strip_model()->ActivateTabAt(0);

    // Window 2 on the right side of the screen
    BrowserWindowInterface* window2 = CreateBrowser(browser()->GetProfile());
    window2->GetWindow()->SetBounds(kWindowBounds2);

    // Window 2 Tab 1 should be pinned and shows GetUrl(1)
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(window2,
                                                              GetUrl(1), 1);
    window2->GetTabStripModel()->SetTabPinned(0, true);

    // Window 2 Tab 2 should show GetUrl(2)
    chrome::AddSelectedTabWithURL(window2, GetUrl(2),
                                  ui::PAGE_TRANSITION_TYPED);
    content::WaitForLoadStop(
        window2->tab_strip_model()->GetActiveWebContents());

    // Window 2 Tab 3 should show GetUrl(3)
    chrome::AddSelectedTabWithURL(window2, GetUrl(3),
                                  ui::PAGE_TRANSITION_TYPED);
    content::WaitForLoadStop(
        window2->tab_strip_model()->GetActiveWebContents());

    // Ensure Window 2 Tab 2 (index 1) is active
    window2->tab_strip_model()->ActivateTabAt(1);

    // Window 2 Tabs 2 and 3 should be part of a tab group with group visual
    // data
    tab_groups::TabGroupId group_id =
        window2->tab_strip_model()->AddToNewGroup({1, 2});
    window2->tab_strip_model()->ChangeTabGroupVisuals(
        group_id, tab_groups::TabGroupVisualData(
                      u"Work", tab_groups::TabGroupColorId::kBlue));

    // ACTIVATE WINDOW 1 (browser()) to make it the active window!
    browser()->GetWindow()->Activate();

    // Allow the window manager to process bounds changes and send messages
    // back. We wait for the sizes to match since positions might be adjusted
    // by the window manager/compositor.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return browser()->GetWindow()->GetBounds().size() ==
                 kWindowBounds1.size() &&
             window2->GetWindow()->GetBounds().size() == kWindowBounds2.size();
    })) << "Actual browser bounds: "
        << browser()->GetWindow()->GetBounds().ToString()
        << ", Expected size: " << kWindowBounds1.size().ToString()
        << "\nActual window2 bounds: "
        << window2->GetWindow()->GetBounds().ToString()
        << ", Expected size: " << kWindowBounds2.size().ToString();

    base::FilePath bounds_file =
        browser()->GetProfile()->GetPath().AppendASCII("saved_bounds.json");
    SaveWindowBoundsToFile(bounds_file, browser(), window2);
  }

  void AssertSessionState() {
    if (SessionRestore::IsRestoring(browser()->GetProfile())) {
      SessionRestoreTestHelper helper;
      helper.Wait();
    }

    auto windows = GetAllBrowserWindowInterfaces();
    std::vector<BrowserWindowInterface*> profile_windows;
    for (auto* window : windows) {
      if (window->GetProfile() == browser()->GetProfile()) {
        profile_windows.push_back(window);
      }
    }

    for (auto* window : profile_windows) {
      CloseAboutBlankTabs(window);
      EnsureTabsLoaded(window->GetTabStripModel());
    }

    BrowserWindowInterface* w1 = nullptr;
    BrowserWindowInterface* w2 = nullptr;
    for (auto* window : profile_windows) {
      if (window->GetTabStripModel()->count() == 2) {
        w1 = window;
      } else if (window->GetTabStripModel()->count() == 3) {
        w2 = window;
      }
    }

    ASSERT_TRUE(w1) << "Window 1 (2 tabs) not found.";
    ASSERT_TRUE(w2) << "Window 2 (3 tabs) not found.";

    base::FilePath bounds_file =
        browser()->GetProfile()->GetPath().AppendASCII("saved_bounds.json");
    auto [expected_bounds1, expected_bounds2] =
        ReadWindowBoundsFromFile(bounds_file);

    // Assert Window 1 bounds
    VerifyWindowBounds(expected_bounds1, w1->GetWindow()->GetBounds());

    // Window 1 Tab 1 shows GetUrl(1) and is active
    TabStripModel* w1_model = w1->GetTabStripModel();
    EXPECT_EQ(0, w1_model->active_index());
    EXPECT_EQ(GetUrl(1), w1_model->GetWebContentsAt(0)->GetURL());

    // Window 1 Tab 2 shows GetUrl(3), with GetUrl(2) in history
    content::WebContents* w1_tab2 = w1_model->GetWebContentsAt(1);
    EXPECT_EQ(GetUrl(3), w1_tab2->GetURL());
    content::NavigationController& controller = w1_tab2->GetController();
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ(GetUrl(2), controller.GetEntryAtIndex(0)->GetURL());
    EXPECT_EQ(GetUrl(3), controller.GetEntryAtIndex(1)->GetURL());

    // Assert Window 2 bounds
    VerifyWindowBounds(expected_bounds2, w2->GetWindow()->GetBounds());

    TabStripModel* w2_model = w2->GetTabStripModel();
    // Tab 1 should be pinned and shows GetUrl(1)
    EXPECT_TRUE(w2_model->IsTabPinned(0));
    EXPECT_EQ(GetUrl(1), w2_model->GetWebContentsAt(0)->GetURL());

    // Tab 2 should show GetUrl(2) and be active
    EXPECT_EQ(1, w2_model->active_index());
    content::WebContents* w2_tab2 = w2_model->GetWebContentsAt(1);
    if (w2_tab2->GetURL() == GURL("chrome://newtab/") ||
        w2_tab2->GetURL() == GURL("chrome://new-tab-page/")) {
      w2_tab2->GetController().GoBack();
      content::WaitForLoadStop(w2_tab2);
    }
    EXPECT_EQ(GetUrl(2), w2_tab2->GetURL());

    // Tab 3 should show GetUrl(3)
    EXPECT_EQ(GetUrl(3), w2_model->GetWebContentsAt(2)->GetURL());

    // Tabs 2 and 3 should be part of a tab group with group visual data
    std::optional<tab_groups::TabGroupId> group_id2 =
        w2_model->GetTabGroupForTab(1);
    std::optional<tab_groups::TabGroupId> group_id3 =
        w2_model->GetTabGroupForTab(2);
    ASSERT_TRUE(group_id2.has_value());
    ASSERT_TRUE(group_id3.has_value());
    EXPECT_EQ(group_id2.value(), group_id3.value());

    const tab_groups::TabGroupVisualData* visual_data =
        w2_model->group_model()->GetTabGroup(group_id2.value())->visual_data();
    ASSERT_TRUE(visual_data);
    EXPECT_EQ(u"Work", visual_data->title());
    EXPECT_EQ(tab_groups::TabGroupColorId::kBlue, visual_data->color());
  }
};

IN_PROC_BROWSER_TEST_P(SessionRestoreAcrossStagesTest, PRE_Restore) {
  SetUpSessionState();
  browser()->GetProfile()->SaveSessionState();
  AssertCommandStorageBackendFilesExist(SessionType::kSessionRestore);
}

// TODO(crbug.com/553933982): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_Restore DISABLED_Restore
#else
#define MAYBE_Restore Restore
#endif
IN_PROC_BROWSER_TEST_P(SessionRestoreAcrossStagesTest, MAYBE_Restore) {
  AssertSessionState();
  browser()->GetProfile()->SaveSessionState();
  AssertCommandStorageBackendFilesExist(SessionType::kSessionRestore);
}

// Tests tab restore behavior across browser restarts, where the encryption
// rollout stage changes between restarts.
class TabRestoreAcrossStagesTest : public RestoreAcrossStagesTestBase {
 public:
  // Creates the expected tabs for the test, which are verified in
  // AssertExpectedTabs().
  void SetUpExpectedTabs() {
    CloseAboutBlankTabs(browser());
    GURL url1 = GetUrl(1);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url1, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    GURL url2 = GetUrl(2);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

    EXPECT_EQ(2, browser()->tab_strip_model()->count());
    EXPECT_EQ(url1,
              browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());
    EXPECT_EQ(url2,
              browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
    CloseTab(1);
    EXPECT_EQ(1, browser()->tab_strip_model()->count());
    // There are now: 1 open tab and 1 restorable closed tab.
  }

  // Asserts that the tabs from SetUpExpectedTabs() are present.
  void AssertExpectedTabs() {
    if (SessionRestore::IsRestoring(browser()->GetProfile())) {
      SessionRestoreTestHelper helper;
      helper.Wait();
    }
    CloseAboutBlankTabs(browser());
    EnsureTabsLoaded(browser()->tab_strip_model());
    int tab_count = browser()->tab_strip_model()->count();
    EXPECT_EQ(1, tab_count);
    if (tab_count >= 1) {
      EXPECT_EQ(GetUrl(1),
                browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());
    }
    ASSERT_NO_FATAL_FAILURE(RestoreTab(browser(), 1));
    EXPECT_EQ(2, browser()->tab_strip_model()->count());
    EXPECT_EQ(GetUrl(2),
              browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
    // Close the tab that was just restored; returning the state to the
    // original 1 open tab + 1 restorable closed tab.
    CloseTab(1);
    EXPECT_EQ(1, browser()->tab_strip_model()->count());
  }
};

IN_PROC_BROWSER_TEST_P(TabRestoreAcrossStagesTest, PRE_Restore) {
  SetUpExpectedTabs();
  browser()->GetProfile()->SaveSessionState();
  AssertCommandStorageBackendFilesExist(SessionType::kTabRestore);
}

IN_PROC_BROWSER_TEST_P(TabRestoreAcrossStagesTest, Restore) {
  AssertExpectedTabs();
  browser()->GetProfile()->SaveSessionState();
  AssertCommandStorageBackendFilesExist(SessionType::kTabRestore);
}

const StageTransitionTestParams kStageTransitionTestParams[] = {
    // Starting a rollout with Stage 1
    {TestParams{false, ""},
     TestParams{true, kEncryptSessionStorageStageWriteBothReadOnlyClear}},
    // Rollback from Stage 1 to Stage 0
    {TestParams{true, kEncryptSessionStorageStageWriteBothReadOnlyClear},
     TestParams{false, ""}},

    // Continuing rollout from Stage 1 to Stage 2
    {TestParams{true, kEncryptSessionStorageStageWriteBothReadOnlyClear},
     TestParams{true, kEncryptSessionStorageStageWriteBothReadPreferEncrypted}},
    // Jump from Stage 0 to Stage 2
    // This could occur if a user misses the rollout from Stage 1 to Stage 2
    {TestParams{false, ""},
     TestParams{true, kEncryptSessionStorageStageWriteBothReadPreferEncrypted}},
    // Rollback from Stage 2 to Stage 1
    {TestParams{true, kEncryptSessionStorageStageWriteBothReadPreferEncrypted},
     TestParams{true, kEncryptSessionStorageStageWriteBothReadOnlyClear}},
    // Rollback from Stage 2 to Stage 0
    {TestParams{true, kEncryptSessionStorageStageWriteBothReadPreferEncrypted},
     TestParams{false, ""}},

    // Continuing rollout from Stage 2 to Stage 3
    {TestParams{true, kEncryptSessionStorageStageWriteBothReadPreferEncrypted},
     TestParams{true,
                kEncryptSessionStorageStageWriteEncryptedReadPreferEncrypted}},
    // Jump from Stage 0 to Stage 3
    // This could occur if a user misses the rollouts to Stages 1 and 2
    {TestParams{false, ""},
     TestParams{true,
                kEncryptSessionStorageStageWriteEncryptedReadPreferEncrypted}},
    // Jump from Stage 1 to Stage 3
    // This could occur if a user misses the rollout to Stage 2
    {TestParams{true, kEncryptSessionStorageStageWriteBothReadOnlyClear},
     TestParams{true,
                kEncryptSessionStorageStageWriteEncryptedReadPreferEncrypted}},
    // Rollback from Stage 3 to Stage 2
    {TestParams{true,
                kEncryptSessionStorageStageWriteEncryptedReadPreferEncrypted},
     TestParams{true, kEncryptSessionStorageStageWriteBothReadPreferEncrypted}},
    // Rollback from Stage 3 to Stage 0 or 1 is not possible because cleartext
    // files are not written in Stage 3 and are required for Stages 0 and 1.
};

INSTANTIATE_TEST_SUITE_P(All,
                         SessionRestoreAcrossStagesTest,
                         testing::ValuesIn(kStageTransitionTestParams),
                         StageTransitionNameGenerator);

INSTANTIATE_TEST_SUITE_P(All,
                         TabRestoreAcrossStagesTest,
                         testing::ValuesIn(kStageTransitionTestParams),
                         StageTransitionNameGenerator);

}  // namespace sessions
