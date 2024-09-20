// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_load_waiter.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/content/content_test_helper.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service_client.h"
#include "components/sessions/core/tab_restore_service_impl.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "ui/base/mojom/window_show_state.mojom.h"

typedef sessions::tab_restore::Entry Entry;
typedef sessions::tab_restore::Tab Tab;
typedef sessions::tab_restore::Window Window;
typedef std::map<std::string, std::string> ExtraData;

using content::NavigationEntry;
using content::WebContentsTester;
using sessions::ContentTestHelper;
using sessions::SerializedNavigationEntry;
using sessions::SerializedNavigationEntryTestHelper;

using ::testing::_;
using ::testing::Return;

class MockLiveTab : public sessions::LiveTab {
 public:
  MockLiveTab() = default;
  ~MockLiveTab() override = default;

  MOCK_METHOD0(IsInitialBlankNavigation, bool());
  MOCK_METHOD0(GetCurrentEntryIndex, int());
  MOCK_METHOD0(GetPendingEntryIndex, int());
  MOCK_METHOD1(GetEntryAtIndex, sessions::SerializedNavigationEntry(int index));
  MOCK_METHOD0(GetPendingEntry, sessions::SerializedNavigationEntry());
  MOCK_METHOD0(GetEntryCount, int());
  MOCK_METHOD0(
      GetPlatformSpecificTabData,
      std::unique_ptr<sessions::tab_restore::PlatformSpecificTabData>());
  MOCK_METHOD0(GetUserAgentOverride, sessions::SerializedUserAgentOverride());
};

class MockLiveTabContext : public sessions::LiveTabContext {
 public:
  MockLiveTabContext() = default;
  ~MockLiveTabContext() override = default;

  MOCK_METHOD0(ShowBrowserWindow, void());
  MOCK_CONST_METHOD0(GetSessionID, SessionID());
  MOCK_CONST_METHOD0(GetWindowType, sessions::SessionWindow::WindowType());
  MOCK_CONST_METHOD0(GetTabCount, int());
  MOCK_CONST_METHOD0(GetSelectedIndex, int());
  MOCK_CONST_METHOD0(GetAppName, std::string());
  MOCK_CONST_METHOD0(GetUserTitle, std::string());
  MOCK_CONST_METHOD1(GetLiveTabAt, sessions::LiveTab*(int index));
  MOCK_CONST_METHOD0(GetActiveLiveTab, sessions::LiveTab*());
  MOCK_CONST_METHOD1(GetExtraDataForTab,
                     std::map<std::string, std::string>(int index));
  MOCK_CONST_METHOD0(GetExtraDataForWindow,
                     std::map<std::string, std::string>());
  MOCK_CONST_METHOD1(GetTabGroupForTab,
                     std::optional<tab_groups::TabGroupId>(int index));
  MOCK_CONST_METHOD1(GetVisualDataForGroup,
                     const tab_groups::TabGroupVisualData*(
                         const tab_groups::TabGroupId& group));
  MOCK_CONST_METHOD1(
      GetSavedTabGroupIdForGroup,
      const std::optional<base::Uuid>(const tab_groups::TabGroupId& group));
  MOCK_CONST_METHOD1(IsTabPinned, bool(int index));
  MOCK_METHOD2(SetVisualDataForGroup,
               void(const tab_groups::TabGroupId& group,
                    const tab_groups::TabGroupVisualData& visual_data));
  MOCK_CONST_METHOD0(GetRestoredBounds, const gfx::Rect());
  MOCK_CONST_METHOD0(GetRestoredState, ui::mojom::WindowShowState());
  MOCK_CONST_METHOD0(GetWorkspace, std::string());
  MOCK_METHOD(sessions::LiveTab*,
              AddRestoredTab,
              ((const sessions::tab_restore::Tab&),
               int,
               bool,
               sessions::tab_restore::Type),
              (override));
  MOCK_METHOD(sessions::LiveTab*,
              ReplaceRestoredTab,
              ((const sessions::tab_restore::Tab&)),
              (override));
  MOCK_METHOD0(CloseTab, void());
};

class MockTabRestoreServiceClient : public sessions::TabRestoreServiceClient {
 public:
  MockTabRestoreServiceClient() = default;
  ~MockTabRestoreServiceClient() override = default;

  MOCK_METHOD8(CreateLiveTabContext,
               sessions::LiveTabContext*(
                   sessions::LiveTabContext* existing_context,
                   sessions::SessionWindow::WindowType type,
                   const std::string& app_name,
                   const gfx::Rect& bounds,
                   ui::mojom::WindowShowState show_state,
                   const std::string& workspace,
                   const std::string& user_title,
                   const std::map<std::string, std::string>& extra_data));
  MOCK_METHOD1(FindLiveTabContextForTab,
               sessions::LiveTabContext*(const sessions::LiveTab* tab));
  MOCK_METHOD1(FindLiveTabContextWithID,
               sessions::LiveTabContext*(SessionID desired_id));
  MOCK_METHOD1(FindLiveTabContextWithGroup,
               sessions::LiveTabContext*(tab_groups::TabGroupId group));
  MOCK_METHOD1(ShouldTrackURLForRestore, bool(const GURL& url));
  MOCK_METHOD1(GetExtensionAppIDForTab, std::string(sessions::LiveTab* tab));
  MOCK_METHOD0(GetPathToSaveTo, base::FilePath());
  MOCK_METHOD0(GetNewTabURL, GURL());
  MOCK_METHOD0(HasLastSession, bool());
  MOCK_METHOD1(GetLastSession, void(sessions::GetLastSessionCallback callback));
  MOCK_METHOD1(OnTabRestored, void(const GURL& url));
};

// Create subclass that overrides TimeNow so that we can control the time used
// for closed tabs and windows.
class TabRestoreTimeFactory : public sessions::tab_restore::TimeFactory {
 public:
  TabRestoreTimeFactory() : time_(base::Time::Now()) {}

  ~TabRestoreTimeFactory() override {}

  base::Time TimeNow() override { return time_; }

 private:
  base::Time time_;
};

class TabRestoreServiceImplTest : public ChromeRenderViewHostTestHarness {
 public:
  TabRestoreServiceImplTest()
      : url1_("http://1"),
        url2_("http://2"),
        url3_("http://3"),
        user_agent_override_(blink::UserAgentOverride::UserAgentOnly(
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/535.19"
            " (KHTML, like Gecko) Chrome/18.0.1025.45 Safari/535.19")),
        time_factory_(nullptr),
        window_id_(SessionID::FromSerializedValue(1)),
        tab_id_(SessionID::FromSerializedValue(2)) {
    user_agent_override_.ua_metadata_override.emplace();
    user_agent_override_.ua_metadata_override->brand_version_list.emplace_back(
        "Chrome", "18");
    user_agent_override_.ua_metadata_override->brand_full_version_list
        .emplace_back("Chrome", "18.0.1025.45");
    user_agent_override_.ua_metadata_override->full_version = "18.0.1025.45";
    user_agent_override_.ua_metadata_override->platform = "Linux";
    user_agent_override_.ua_metadata_override->architecture = "x86_64";
    user_agent_override_.ua_metadata_override->bitness = "32";
  }

  ~TabRestoreServiceImplTest() override {}

  SessionID tab_id() const { return tab_id_; }
  SessionID window_id() const { return window_id_; }

 protected:
  enum {
    kMaxEntries = sessions::TabRestoreServiceHelper::kMaxEntries,
  };

  // testing::Test:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    live_tab_ = base::WrapUnique(new sessions::ContentLiveTab(web_contents()));
    time_factory_ = new TabRestoreTimeFactory();

    CreateService();
  }

  void TearDown() override {
    service_->Shutdown();
    service_.reset();
    delete time_factory_;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  sessions::TabRestoreService::Entries* mutable_entries() {
    return service_->mutable_entries();
  }

  void PruneEntries() { service_->PruneEntries(); }

  void AddThreeNavigations() {
    // Navigate to three URLs.
    NavigateAndCommit(url1_);
    NavigateAndCommit(url2_);
    NavigateAndCommit(url3_);
  }

  void NavigateToIndex(int index) {
    // Navigate back. We have to do this song and dance as NavigationController
    // isn't happy if you navigate immediately while going back.
    controller().GoToIndex(index);
    WebContentsTester::For(web_contents())->CommitPendingNavigation();
  }

  virtual void CreateService() {
    service_ = std::make_unique<sessions::TabRestoreServiceImpl>(
        std::make_unique<ChromeTabRestoreServiceClient>(profile()),
        profile()->GetPrefs(), time_factory_);
  }

  void RecreateService() {
    // Must set service to null first so that it is destroyed before the new
    // one is created.
    service_->Shutdown();
    content::RunAllTasksUntilIdle();
    service_.reset();

    CreateService();
    SynchronousLoadTabsFromLastSession();
  }

  // Adds a window with one tab and url to the profile's session
  // service. If |pinned| is true, the tab is marked as pinned in the
  // session service. If |group| is present, sets the tab's group ID. If
  // |group_visual_data| is also present, sets |group|'s visual data.
  void AddWindowWithOneTabToSessionService(
      bool pinned,
      std::optional<tab_groups::TabGroupId> group = std::nullopt,
      std::optional<tab_groups::TabGroupVisualData> group_visual_data =
          std::nullopt,
      absl ::optional<ExtraData> extra_data = std::nullopt) {
    // Create new window / tab IDs so that these remain distinct.
    window_id_ = SessionID::NewUnique();
    tab_id_ = SessionID::NewUnique();

    SessionService* session_service =
        SessionServiceFactory::GetForProfile(profile());
    session_service->SetWindowType(window_id(), Browser::TYPE_NORMAL);
    session_service->SetTabWindow(window_id(), tab_id());
    session_service->SetTabIndexInWindow(window_id(), tab_id(), 0);
    session_service->SetSelectedTabInWindow(window_id(), 0);
    if (pinned)
      session_service->SetPinnedState(window_id(), tab_id(), true);
    if (group)
      session_service->SetTabGroup(window_id(), tab_id(), group);
    if (group && group_visual_data)
      session_service->SetTabGroupMetadata(window_id(), *group,
                                           &*group_visual_data);

    session_service->UpdateTabNavigation(
        window_id(), tab_id(),
        ContentTestHelper::CreateNavigation(url1_.spec(), "title"));
  }

  // Creates a SessionService and assigns it to the Profile. The SessionService
  // is configured with a single window with a single tab pointing at url1_ by
  // way of AddWindowWithOneTabToSessionService. If |pinned| is true, the
  // tab is marked as pinned in the session service.
  void CreateSessionServiceWithOneWindow(bool pinned) {
    std::unique_ptr<SessionService> session_service(
        new SessionService(profile()));
    SessionServiceFactory::SetForTestProfile(profile(),
                                             std::move(session_service));

    AddWindowWithOneTabToSessionService(pinned);

    // Set this, otherwise previous session won't be loaded.
    ExitTypeService::GetInstanceForProfile(profile())
        ->SetLastSessionExitTypeForTest(ExitType::kCrashed);
  }

  void SynchronousLoadTabsFromLastSession() {
    // Ensures that the load is complete before continuing.
    TabRestoreServiceLoadWaiter waiter(service_.get());
    service_->LoadTabsFromLastSession();
    waiter.Wait();
  }

  sessions::LiveTab* live_tab() { return live_tab_.get(); }

  GURL url1_;
  GURL url2_;
  GURL url3_;
  blink::UserAgentOverride user_agent_override_;
  std::unique_ptr<sessions::LiveTab> live_tab_;
  std::unique_ptr<sessions::TabRestoreServiceImpl> service_;
  raw_ptr<TabRestoreTimeFactory, DanglingUntriaged> time_factory_;
  SessionID window_id_;
  SessionID tab_id_;
};

class TabRestoreServiceImplWithMockClientTest
    : public TabRestoreServiceImplTest {
 public:
  TabRestoreServiceImplWithMockClientTest() = default;
  ~TabRestoreServiceImplWithMockClientTest() override = default;

 protected:
  void CreateService() override {
    std::unique_ptr<MockTabRestoreServiceClient> service_client =
        std::make_unique<testing::NiceMock<MockTabRestoreServiceClient>>();
    mock_tab_restore_service_client_ = service_client.get();
    ON_CALL(*mock_tab_restore_service_client_, GetPathToSaveTo())
        .WillByDefault(Return(profile()->GetPath()));

    service_ = std::make_unique<sessions::TabRestoreServiceImpl>(
        std::move(service_client), profile()->GetPrefs(), time_factory_);
  }

  raw_ptr<MockTabRestoreServiceClient, DanglingUntriaged>
      mock_tab_restore_service_client_;
};

TEST_F(TabRestoreServiceImplTest, Basic) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  // Make sure an entry was created.
  ASSERT_EQ(1U, service_->entries().size());

  // Make sure the entry matches.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  EXPECT_FALSE(tab->pinned);
  EXPECT_TRUE(tab->extension_app_id.empty());
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_TRUE(url1_ == tab->navigations[0].virtual_url());
  EXPECT_TRUE(url2_ == tab->navigations[1].virtual_url());
  EXPECT_TRUE(url3_ == tab->navigations[2].virtual_url());
  EXPECT_EQ("", tab->user_agent_override.ua_string_override);
  EXPECT_TRUE(!blink::UserAgentMetadata::Demarshal(
                   tab->user_agent_override.opaque_ua_metadata_override)
                   .has_value());
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_EQ(
      time_factory_->TimeNow().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      tab->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());

  NavigateToIndex(1);

  // And check again, but set the user agent override this time.
  web_contents()->SetUserAgentOverride(user_agent_override_, false);
  service_->CreateHistoricalTab(live_tab(), -1);

  // There should be two entries now.
  ASSERT_EQ(2U, service_->entries().size());

  // Make sure the entry matches.
  entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, entry->type);
  tab = static_cast<Tab*>(entry);
  EXPECT_FALSE(tab->pinned);
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_EQ(url1_, tab->navigations[0].virtual_url());
  EXPECT_EQ(url2_, tab->navigations[1].virtual_url());
  EXPECT_EQ(url3_, tab->navigations[2].virtual_url());
  EXPECT_EQ(user_agent_override_.ua_string_override,
            tab->user_agent_override.ua_string_override);
  std::optional<blink::UserAgentMetadata> client_hints_override =
      blink::UserAgentMetadata::Demarshal(
          tab->user_agent_override.opaque_ua_metadata_override);
  EXPECT_EQ(user_agent_override_.ua_metadata_override, client_hints_override);
  EXPECT_EQ(1, tab->current_navigation_index);
  EXPECT_EQ(
      time_factory_->TimeNow().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      tab->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

TEST_F(TabRestoreServiceImplWithMockClientTest,
       TabExtraDataPresentInHistoricalTab) {
  constexpr char kSampleKey[] = "test";
  constexpr char kSampleValue[] = "true";

  std::unique_ptr<MockLiveTabContext> mock_live_tab_context_ptr(
      new ::testing::NiceMock<MockLiveTabContext>());
  SessionID sample_session_id = SessionID::NewUnique();
  EXPECT_CALL(*mock_live_tab_context_ptr, GetSessionID)
      .WillOnce(Return(sample_session_id));
  EXPECT_CALL(*mock_live_tab_context_ptr, GetExtraDataForTab)
      .WillOnce([kSampleKey, kSampleValue]() {
        std::map<std::string, std::string> sample_extra_data;
        sample_extra_data[kSampleKey] = kSampleValue;
        return sample_extra_data;
      });
  ON_CALL(*mock_tab_restore_service_client_, FindLiveTabContextForTab(_))
      .WillByDefault(Return(mock_live_tab_context_ptr.get()));
  ON_CALL(*mock_tab_restore_service_client_, GetNewTabURL())
      .WillByDefault(Return(GURL("https://www.google.com")));

  NavigateAndCommit(url1_);
  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  // Make sure an entry was created.
  ASSERT_EQ(1U, service_->entries().size());
  // Make sure the entry data matches.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  ASSERT_EQ(1U, tab->navigations.size());
  EXPECT_EQ(url1_, tab->navigations[0].virtual_url());
  ASSERT_EQ(1U, tab->extra_data.size());
  ASSERT_EQ(kSampleValue, tab->extra_data[kSampleKey]);
}

// Ensure fields are written and read from saved state.
TEST_F(TabRestoreServiceImplWithMockClientTest, WindowRestore) {
  ON_CALL(*mock_tab_restore_service_client_, ShouldTrackURLForRestore(_))
      .WillByDefault(Return(true));

  SerializedNavigationEntry navigation_entry =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  testing::NiceMock<MockLiveTab> mock_live_tab;
  ON_CALL(mock_live_tab, GetEntryCount).WillByDefault(Return(1));
  ON_CALL(mock_live_tab, GetEntryAtIndex)
      .WillByDefault(Return(navigation_entry));

  testing::NiceMock<MockLiveTabContext> mock_live_tab_context;
  SessionID session_id = SessionID::NewUnique();
  ON_CALL(mock_live_tab_context, GetSessionID)
      .WillByDefault(Return(session_id));
  ON_CALL(mock_live_tab_context, GetWindowType)
      .WillByDefault(Return(sessions::SessionWindow::TYPE_APP_POPUP));
  ON_CALL(mock_live_tab_context, GetAppName).WillByDefault(Return("app-name"));
  ON_CALL(mock_live_tab_context, GetUserTitle)
      .WillByDefault(Return("user-title"));
  ON_CALL(mock_live_tab_context, GetRestoredBounds)
      .WillByDefault(Return(gfx::Rect(10, 20, 30, 40)));
  ON_CALL(mock_live_tab_context, GetRestoredState)
      .WillByDefault(Return(ui::mojom::WindowShowState::kMaximized));
  ON_CALL(mock_live_tab_context, GetWorkspace)
      .WillByDefault(Return("workspace"));
  ON_CALL(mock_live_tab_context, GetTabCount).WillByDefault(Return(1));
  ON_CALL(mock_live_tab_context, GetLiveTabAt)
      .WillByDefault(Return(&mock_live_tab));

  service_->BrowserClosing(&mock_live_tab_context);

  // Validate while entries are in memory.
  auto validate = [&]() {
    ASSERT_EQ(1u, service_->entries().size());
    Entry* entry = service_->entries().front().get();
    EXPECT_EQ(sessions::tab_restore::Type::WINDOW, entry->type);
    Window* window = static_cast<Window*>(entry);
    EXPECT_EQ(sessions::SessionWindow::TYPE_APP_POPUP, window->type);
    EXPECT_EQ(0, window->selected_tab_index);
    EXPECT_EQ("app-name", window->app_name);
    EXPECT_EQ("user-title", window->user_title);
    EXPECT_EQ(gfx::Rect(10, 20, 30, 40), window->bounds);
    EXPECT_EQ(ui::mojom::WindowShowState::kMaximized, window->show_state);
    EXPECT_EQ("workspace", window->workspace);
  };
  validate();

  // Validate after persisting and reading from storage.
  RecreateService();
  validate();
}

// Make sure TabRestoreService doesn't create an entry for a tab with no
// navigations.
TEST_F(TabRestoreServiceImplTest, DontCreateEmptyTab) {
  service_->CreateHistoricalTab(live_tab(), -1);
  EXPECT_TRUE(service_->entries().empty());
}

// Tests restoring a single tab.
TEST_F(TabRestoreServiceImplTest, Restore) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);
  EXPECT_EQ(1U, service_->entries().size());

  // Recreate the service and have it load the tabs.
  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // And verify the entry.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  EXPECT_FALSE(tab->pinned);
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_TRUE(url1_ == tab->navigations[0].virtual_url());
  EXPECT_TRUE(url2_ == tab->navigations[1].virtual_url());
  EXPECT_TRUE(url3_ == tab->navigations[2].virtual_url());
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_EQ(
      time_factory_->TimeNow().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      tab->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

// Tests restoring a tab with more than gMaxPersistNavigationCount entries.
TEST_F(TabRestoreServiceImplTest, RestoreManyNavigations) {
  AddThreeNavigations();
  AddThreeNavigations();
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  // Recreate the service and have it load the tabs.
  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // And verify the entry.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  // Only gMaxPersistNavigationCount + 1 (current navigation) are persisted.
  ASSERT_EQ(7U, tab->navigations.size());
  // Check that they are created with correct indices.
  EXPECT_EQ(0, tab->navigations[0].index());
  EXPECT_EQ(6, tab->navigations[6].index());
  EXPECT_EQ(6, tab->current_navigation_index);
}

// Tests restoring a single pinned tab.
TEST_F(TabRestoreServiceImplTest, RestorePinnedAndApp) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // We have to explicitly mark the tab as pinned as there is no browser for
  // these tests.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  tab->pinned = true;
  const std::string extension_app_id("test");
  tab->extension_app_id = extension_app_id;

  // Recreate the service and have it load the tabs.
  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // And verify the entry.
  entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, entry->type);
  tab = static_cast<Tab*>(entry);
  EXPECT_TRUE(tab->pinned);
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_TRUE(url1_ == tab->navigations[0].virtual_url());
  EXPECT_TRUE(url2_ == tab->navigations[1].virtual_url());
  EXPECT_TRUE(url3_ == tab->navigations[2].virtual_url());
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_TRUE(extension_app_id == tab->extension_app_id);
}

// Make sure TabRestoreService doesn't create a restored entry.
TEST_F(TabRestoreServiceImplTest, DontCreateRestoredEntry) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);
  EXPECT_EQ(1U, service_->entries().size());

  // Record the tab's id.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, entry->type);
  SessionID first_id = entry->id;

  // Service record the second tab.
  service_->CreateHistoricalTab(live_tab(), -1);
  EXPECT_EQ(2U, service_->entries().size());

  // Record the tab's id.
  entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, entry->type);
  SessionID second_id = entry->id;

  service_->Shutdown();

  // Add a restored entry command
  service_->CreateRestoredEntryCommandForTest(second_id);

  // Recreate the service
  RecreateService();

  // Only one entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // And verify the entry.
  entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, entry->type);
  ASSERT_EQ(first_id, entry->original_id);
}

// Tests deleting entries.
TEST_F(TabRestoreServiceImplTest, DeleteNavigationEntries) {
  SynchronousLoadTabsFromLastSession();
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  service_->DeleteNavigationEntries(
      base::BindLambdaForTesting([&](const SerializedNavigationEntry& entry) {
        return entry.virtual_url() == url2_;
      }));

  // The entry should still exist but url2_ was removed and indices adjusted.
  ASSERT_EQ(1U, service_->entries().size());
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  ASSERT_EQ(2U, tab->navigations.size());
  EXPECT_EQ(url1_, tab->navigations[0].virtual_url());
  EXPECT_EQ(0, tab->navigations[0].index());
  EXPECT_EQ(url3_, tab->navigations[1].virtual_url());
  EXPECT_EQ(1, tab->navigations[1].index());
  EXPECT_EQ(1, tab->current_navigation_index);

  service_->DeleteNavigationEntries(base::BindRepeating(
      [](const SerializedNavigationEntry& entry) { return true; }));

  // The entry should be removed.
  EXPECT_EQ(0U, service_->entries().size());
}

// Tests deleting entries.
TEST_F(TabRestoreServiceImplTest, DeleteCurrentEntry) {
  SynchronousLoadTabsFromLastSession();
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  service_->DeleteNavigationEntries(
      base::BindLambdaForTesting([&](const SerializedNavigationEntry& entry) {
        return entry.virtual_url() == url3_;
      }));

  // The entry should be deleted because the current url was deleted.
  EXPECT_EQ(0U, service_->entries().size());
}

// Tests deleting entries.
TEST_F(TabRestoreServiceImplTest, DeleteEntriesAndRecreate) {
  SynchronousLoadTabsFromLastSession();
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  // Delete the navigation for url2_.
  service_->DeleteNavigationEntries(
      base::BindLambdaForTesting([&](const SerializedNavigationEntry& entry) {
        return entry.virtual_url() == url2_;
      }));
  // Recreate the service and have it load the tabs.
  RecreateService();
  // The entry should still exist but url2_ was removed and indices adjusted.
  ASSERT_EQ(1U, service_->entries().size());
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  ASSERT_EQ(2U, tab->navigations.size());
  EXPECT_EQ(url1_, tab->navigations[0].virtual_url());
  EXPECT_EQ(0, tab->navigations[0].index());
  EXPECT_EQ(url3_, tab->navigations[1].virtual_url());
  EXPECT_EQ(1, tab->navigations[1].index());
  EXPECT_EQ(1, tab->current_navigation_index);

  // Delete all entries.
  service_->DeleteNavigationEntries(base::BindRepeating(
      [](const SerializedNavigationEntry& entry) { return true; }));
  // Recreate the service and have it load the tabs.
  RecreateService();
  // The entry should be removed.
  ASSERT_EQ(0U, service_->entries().size());
}

// Make sure we persist entries to disk that have post data.
TEST_F(TabRestoreServiceImplTest, DontPersistPostData) {
  AddThreeNavigations();
  controller().GetEntryAtIndex(0)->SetHasPostData(true);
  controller().GetEntryAtIndex(1)->SetHasPostData(true);
  controller().GetEntryAtIndex(2)->SetHasPostData(true);

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);
  ASSERT_EQ(1U, service_->entries().size());

  // Recreate the service and have it load the tabs.
  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  const Entry* restored_entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, restored_entry->type);

  const Tab* restored_tab = static_cast<const Tab*>(restored_entry);
  // There should be 3 navs.
  ASSERT_EQ(3U, restored_tab->navigations.size());
  EXPECT_EQ(
      time_factory_->TimeNow().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      restored_tab->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

// Make sure we don't persist entries to disk that have post data. This
// differs from DontPersistPostData1 in that all the navigations have post
// data, so that nothing should be persisted.
TEST_F(TabRestoreServiceImplTest, DontLoadTwice) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);
  ASSERT_EQ(1U, service_->entries().size());

  // Recreate the service and have it load the tabs.
  RecreateService();

  SynchronousLoadTabsFromLastSession();

  // There should only be one entry.
  ASSERT_EQ(1U, service_->entries().size());
}

// Makes sure we load the previous session as necessary.
TEST_F(TabRestoreServiceImplTest, LoadPreviousSession) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  EXPECT_FALSE(service_->IsLoaded());

  SynchronousLoadTabsFromLastSession();

  // Make sure we get back one entry with one tab whose url is url1.
  ASSERT_EQ(1U, service_->entries().size());
  Entry* entry2 = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::WINDOW, entry2->type);
  sessions::tab_restore::Window* window =
      static_cast<sessions::tab_restore::Window*>(entry2);
  EXPECT_EQ(sessions::SessionWindow::TYPE_NORMAL, window->type);
  ASSERT_EQ(1U, window->tabs.size());
  EXPECT_EQ(0, window->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_EQ(0, window->selected_tab_index);
  ASSERT_EQ(1U, window->tabs[0]->navigations.size());
  EXPECT_EQ(0, window->tabs[0]->current_navigation_index);
  EXPECT_EQ(
      0,
      window->tabs[0]->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_TRUE(url1_ == window->tabs[0]->navigations[0].virtual_url());
}

// Makes sure we don't attempt to load previous sessions after a restore.
TEST_F(TabRestoreServiceImplTest, DontLoadAfterRestore) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  profile()->set_restored_last_session(true);

  SynchronousLoadTabsFromLastSession();

  // Because we restored a session TabRestoreService shouldn't load the tabs.
  ASSERT_EQ(0U, service_->entries().size());
}

// Makes sure we don't attempt to load previous sessions after a clean exit.
TEST_F(TabRestoreServiceImplTest, DontLoadAfterCleanExit) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  ExitTypeService::GetInstanceForProfile(profile())
      ->SetLastSessionExitTypeForTest(ExitType::kClean);

  SynchronousLoadTabsFromLastSession();

  ASSERT_EQ(0U, service_->entries().size());
}

// Makes sure we don't save sessions when saving history is disabled.
TEST_F(TabRestoreServiceImplTest, DontSaveWhenSavingIsDisabled) {
  profile()->GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled, true);

  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  SynchronousLoadTabsFromLastSession();

  ASSERT_EQ(0U, service_->entries().size());
}

// Makes sure we don't attempt to load previous sessions when saving history is
// disabled.
TEST_F(TabRestoreServiceImplTest, DontLoadWhenSavingIsDisabled) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  profile()->GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled, true);

  SynchronousLoadTabsFromLastSession();

  ASSERT_EQ(0U, service_->entries().size());
}

// Regression test to ensure Window::show_state is set correctly when reading
// TabRestoreSession from saved state.
TEST_F(TabRestoreServiceImplTest, WindowShowStateIsSet) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  SynchronousLoadTabsFromLastSession();

  RecreateService();

  // There should be at least one window and its show state should be the
  // default.
  bool got_window = false;
  for (auto& entry : service_->entries()) {
    if (entry->type == sessions::tab_restore::Type::WINDOW) {
      got_window = true;
      Window* window = static_cast<Window*>(entry.get());
      EXPECT_EQ(window->show_state, ui::mojom::WindowShowState::kDefault);
    }
  }
  EXPECT_TRUE(got_window);
}

TEST_F(TabRestoreServiceImplTest, LoadPreviousSessionAndTabs) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  AddThreeNavigations();

  service_->CreateHistoricalTab(live_tab(), -1);

  RecreateService();

  // We should get back two entries, one from the previous session and one from
  // the tab restore service. The previous session entry should be first.
  ASSERT_EQ(2U, service_->entries().size());
  // The first entry should come from the session service.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::WINDOW, entry->type);
  sessions::tab_restore::Window* window =
      static_cast<sessions::tab_restore::Window*>(entry);
  ASSERT_EQ(1U, window->tabs.size());
  EXPECT_EQ(0, window->selected_tab_index);
  EXPECT_EQ(0, window->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  ASSERT_EQ(1U, window->tabs[0]->navigations.size());
  EXPECT_EQ(0, window->tabs[0]->current_navigation_index);
  EXPECT_EQ(
      0,
      window->tabs[0]->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_TRUE(url1_ == window->tabs[0]->navigations[0].virtual_url());

  // Then the closed tab.
  entry = (++service_->entries().begin())->get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  ASSERT_FALSE(tab->pinned);
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_EQ(
      time_factory_->TimeNow().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      tab->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_TRUE(url1_ == tab->navigations[0].virtual_url());
  EXPECT_TRUE(url2_ == tab->navigations[1].virtual_url());
  EXPECT_TRUE(url3_ == tab->navigations[2].virtual_url());
}

// Make sure window bounds and workspace are properly loaded from the session
// service.
TEST_F(TabRestoreServiceImplTest, LoadWindowBoundsAndWorkspace) {
  constexpr gfx::Rect kBounds(10, 20, 640, 480);
  constexpr ui::mojom::WindowShowState kShowState =
      ui::mojom::WindowShowState::kMinimized;
  constexpr char kWorkspace[] = "workspace";

  CreateSessionServiceWithOneWindow(false);

  // Set the bounds, show state and workspace.
  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile());
  session_service->SetWindowBounds(window_id(), kBounds, kShowState);
  session_service->SetWindowWorkspace(window_id(), kWorkspace);

  session_service->MoveCurrentSessionToLastSession();

  AddThreeNavigations();

  service_->CreateHistoricalTab(live_tab(), -1);

  RecreateService();

  // We should get back two entries, one from the previous session and one from
  // the tab restore service. The previous session entry should be first.
  ASSERT_EQ(2U, service_->entries().size());

  // The first entry should come from the session service.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::WINDOW, entry->type);
  sessions::tab_restore::Window* window =
      static_cast<sessions::tab_restore::Window*>(entry);
  ASSERT_EQ(kBounds, window->bounds);
  ASSERT_EQ(kShowState, window->show_state);
  ASSERT_EQ(kWorkspace, window->workspace);
  ASSERT_EQ(1U, window->tabs.size());
  EXPECT_EQ(0, window->selected_tab_index);
  EXPECT_FALSE(window->tabs[0]->pinned);
  ASSERT_EQ(1U, window->tabs[0]->navigations.size());
  EXPECT_EQ(0, window->tabs[0]->current_navigation_index);
  EXPECT_TRUE(url1_ == window->tabs[0]->navigations[0].virtual_url());

  // Then the closed tab.
  entry = (++service_->entries().begin())->get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  ASSERT_FALSE(tab->pinned);
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_TRUE(url1_ == tab->navigations[0].virtual_url());
  EXPECT_TRUE(url2_ == tab->navigations[1].virtual_url());
  EXPECT_TRUE(url3_ == tab->navigations[2].virtual_url());
}

// Make sure pinned state is correctly loaded from session service.
TEST_F(TabRestoreServiceImplTest, LoadPreviousSessionAndTabsPinned) {
  CreateSessionServiceWithOneWindow(true);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  AddThreeNavigations();

  service_->CreateHistoricalTab(live_tab(), -1);

  RecreateService();

  // We should get back two entries, one from the previous session and one from
  // the tab restore service. The previous session entry should be first.
  ASSERT_EQ(2U, service_->entries().size());
  // The first entry should come from the session service.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::WINDOW, entry->type);
  sessions::tab_restore::Window* window =
      static_cast<sessions::tab_restore::Window*>(entry);
  ASSERT_EQ(1U, window->tabs.size());
  EXPECT_EQ(0, window->selected_tab_index);
  EXPECT_TRUE(window->tabs[0]->pinned);
  ASSERT_EQ(1U, window->tabs[0]->navigations.size());
  EXPECT_EQ(0, window->tabs[0]->current_navigation_index);
  EXPECT_TRUE(url1_ == window->tabs[0]->navigations[0].virtual_url());

  // Then the closed tab.
  entry = (++service_->entries().begin())->get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  ASSERT_FALSE(tab->pinned);
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_TRUE(url1_ == tab->navigations[0].virtual_url());
  EXPECT_TRUE(url2_ == tab->navigations[1].virtual_url());
  EXPECT_TRUE(url3_ == tab->navigations[2].virtual_url());
}

// Creates kMaxEntries + 1 windows in the session service and makes sure we only
// get back kMaxEntries on restore.
TEST_F(TabRestoreServiceImplTest, ManyWindowsInSessionService) {
  CreateSessionServiceWithOneWindow(false);

  for (size_t i = 0; i < kMaxEntries; ++i)
    AddWindowWithOneTabToSessionService(false);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  AddThreeNavigations();

  service_->CreateHistoricalTab(live_tab(), -1);

  RecreateService();

  // We should get back kMaxEntries entries. We added more, but
  // TabRestoreService only allows up to kMaxEntries.
  ASSERT_EQ(static_cast<size_t>(kMaxEntries), service_->entries().size());

  // The first entry should come from the session service.
  Entry* entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::WINDOW, entry->type);
  sessions::tab_restore::Window* window =
      static_cast<sessions::tab_restore::Window*>(entry);
  ASSERT_EQ(1U, window->tabs.size());
  EXPECT_EQ(0, window->selected_tab_index);
  EXPECT_EQ(0, window->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  ASSERT_EQ(1U, window->tabs[0]->navigations.size());
  EXPECT_EQ(0, window->tabs[0]->current_navigation_index);
  EXPECT_EQ(
      0,
      window->tabs[0]->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_TRUE(url1_ == window->tabs[0]->navigations[0].virtual_url());
}

// Makes sure we restore timestamps correctly.
TEST_F(TabRestoreServiceImplTest, TimestampSurvivesRestore) {
  base::Time tab_timestamp(
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(123456789)));

  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  // Make sure an entry was created.
  ASSERT_EQ(1U, service_->entries().size());

  // Make sure the entry matches.
  std::vector<SerializedNavigationEntry> old_navigations;
  {
    // |entry|/|tab| doesn't survive after RecreateService().
    Entry* entry = service_->entries().front().get();
    ASSERT_EQ(sessions::tab_restore::Type::TAB, entry->type);
    Tab* tab = static_cast<Tab*>(entry);
    tab->timestamp = tab_timestamp;
    old_navigations = tab->navigations;
  }

  EXPECT_EQ(3U, old_navigations.size());
  for (size_t i = 0; i < old_navigations.size(); ++i) {
    EXPECT_FALSE(old_navigations[i].timestamp().is_null());
  }

  // Set this, otherwise previous session won't be loaded.
  ExitTypeService::GetInstanceForProfile(profile())
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);

  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // And verify the entry.
  Entry* restored_entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, restored_entry->type);
  Tab* restored_tab = static_cast<Tab*>(restored_entry);
  EXPECT_EQ(
      tab_timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds(),
      restored_tab->timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  ASSERT_EQ(old_navigations.size(), restored_tab->navigations.size());
  for (size_t i = 0; i < restored_tab->navigations.size(); ++i) {
    EXPECT_EQ(old_navigations[i].timestamp(),
              restored_tab->navigations[i].timestamp());
  }
}

// Makes sure we restore status codes correctly.
TEST_F(TabRestoreServiceImplTest, StatusCodesSurviveRestore) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(live_tab(), -1);

  // Make sure an entry was created.
  ASSERT_EQ(1U, service_->entries().size());

  // Make sure the entry matches.
  std::vector<sessions::SerializedNavigationEntry> old_navigations;
  {
    // |entry|/|tab| doesn't survive after RecreateService().
    Entry* entry = service_->entries().front().get();
    ASSERT_EQ(sessions::tab_restore::Type::TAB, entry->type);
    Tab* tab = static_cast<Tab*>(entry);
    old_navigations = tab->navigations;
  }

  EXPECT_EQ(3U, old_navigations.size());
  for (size_t i = 0; i < old_navigations.size(); ++i) {
    EXPECT_EQ(200, old_navigations[i].http_status_code());
  }

  // Set this, otherwise previous session won't be loaded.
  ExitTypeService::GetInstanceForProfile(profile())
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);

  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // And verify the entry.
  Entry* restored_entry = service_->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::TAB, restored_entry->type);
  Tab* restored_tab = static_cast<Tab*>(restored_entry);
  ASSERT_EQ(old_navigations.size(), restored_tab->navigations.size());
  for (size_t i = 0; i < restored_tab->navigations.size(); ++i) {
    EXPECT_EQ(200, restored_tab->navigations[i].http_status_code());
  }
}

TEST_F(TabRestoreServiceImplTest, PruneEntries) {
  service_->ClearEntries();
  ASSERT_TRUE(service_->entries().empty());

  const size_t max_entries = kMaxEntries;
  for (size_t i = 0; i < max_entries + 5; i++) {
    SerializedNavigationEntry navigation = ContentTestHelper::CreateNavigation(
        base::StringPrintf("http://%d", static_cast<int>(i)),
        base::NumberToString(i));

    auto tab = std::make_unique<Tab>();
    tab->navigations.push_back(navigation);
    tab->current_navigation_index = 0;

    mutable_entries()->push_back(std::move(tab));
  }

  // Only keep kMaxEntries around.
  EXPECT_EQ(max_entries + 5, service_->entries().size());
  PruneEntries();
  EXPECT_EQ(max_entries, service_->entries().size());
  // Pruning again does nothing.
  PruneEntries();
  EXPECT_EQ(max_entries, service_->entries().size());

  // Prune older first.
  const char kRecentUrl[] = "http://recent";
  SerializedNavigationEntry navigation =
      ContentTestHelper::CreateNavigation(kRecentUrl, "Most recent");
  auto tab = std::make_unique<Tab>();
  tab->navigations.push_back(navigation);
  tab->current_navigation_index = 0;
  mutable_entries()->push_front(std::move(tab));
  EXPECT_EQ(max_entries + 1, service_->entries().size());
  PruneEntries();
  EXPECT_EQ(max_entries, service_->entries().size());
  EXPECT_EQ(GURL(kRecentUrl), static_cast<Tab&>(*service_->entries().front())
                                  .navigations[0]
                                  .virtual_url());

  // Ignore NTPs.
  navigation = ContentTestHelper::CreateNavigation(chrome::kChromeUINewTabURL,
                                                   "New tab");

  tab = std::make_unique<Tab>();
  tab->navigations.push_back(navigation);
  tab->current_navigation_index = 0;
  mutable_entries()->push_front(std::move(tab));

  EXPECT_EQ(max_entries + 1, service_->entries().size());
  PruneEntries();
  EXPECT_EQ(max_entries, service_->entries().size());
  EXPECT_EQ(GURL(kRecentUrl), static_cast<Tab&>(*service_->entries().front())
                                  .navigations[0]
                                  .virtual_url());

  // Don't prune pinned NTPs.
  tab = std::make_unique<Tab>();
  tab->pinned = true;
  tab->current_navigation_index = 0;
  tab->navigations.push_back(navigation);
  mutable_entries()->push_front(std::move(tab));
  EXPECT_EQ(max_entries + 1, service_->entries().size());
  PruneEntries();
  EXPECT_EQ(max_entries, service_->entries().size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            static_cast<Tab*>(service_->entries().front().get())
                ->navigations[0]
                .virtual_url());

  // Don't prune NTPs that have multiple navigations.
  // (Erase the last NTP first.)
  mutable_entries()->erase(mutable_entries()->begin());
  tab = std::make_unique<Tab>();
  tab->current_navigation_index = 1;
  tab->navigations.push_back(navigation);
  tab->navigations.push_back(navigation);
  mutable_entries()->push_front(std::move(tab));
  EXPECT_EQ(max_entries, service_->entries().size());
  PruneEntries();
  EXPECT_EQ(max_entries, service_->entries().size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            static_cast<Tab*>(service_->entries().front().get())
                ->navigations[1]
                .virtual_url());
}

// Regression test for crbug.com/106082
TEST_F(TabRestoreServiceImplTest, PruneIsCalled) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();

  profile()->set_restored_last_session(true);

  const size_t max_entries = kMaxEntries;
  for (size_t i = 0; i < max_entries + 5; i++) {
    NavigateAndCommit(
        GURL(base::StringPrintf("http://%d", static_cast<int>(i))));
    service_->CreateHistoricalTab(live_tab(), -1);
  }

  EXPECT_EQ(max_entries, service_->entries().size());
  // This should not crash.
  SynchronousLoadTabsFromLastSession();
  EXPECT_EQ(max_entries, service_->entries().size());
}

// Makes sure invoking LoadTabsFromLastSession() when the max number of entries
// have been added results in IsLoaded() returning true and notifies observers.
TEST_F(TabRestoreServiceImplTest, GoToLoadedWhenHaveMaxEntries) {
  const size_t max_entries = kMaxEntries;
  for (size_t i = 0; i < max_entries + 5; i++) {
    NavigateAndCommit(
        GURL(base::StringPrintf("http://%d", static_cast<int>(i))));
    service_->CreateHistoricalTab(live_tab(), -1);
  }

  EXPECT_FALSE(service_->IsLoaded());
  EXPECT_EQ(max_entries, service_->entries().size());
  SynchronousLoadTabsFromLastSession();
  EXPECT_TRUE(service_->IsLoaded());
}

// Ensures tab group data is restored from previous session.
TEST_F(TabRestoreServiceImplTest, TabGroupsRestoredFromSessionData) {
  CreateSessionServiceWithOneWindow(false);

  auto group = tab_groups::TabGroupId::GenerateNew();
  auto group_visual_data = tab_groups::TabGroupVisualData(
      u"Foo", tab_groups::TabGroupColorId::kBlue);
  AddWindowWithOneTabToSessionService(false, group, group_visual_data);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();
  EXPECT_FALSE(service_->IsLoaded());
  SynchronousLoadTabsFromLastSession();

  ASSERT_EQ(2u, service_->entries().size());
  Entry* entry = service_->entries().back().get();
  ASSERT_EQ(sessions::tab_restore::Type::WINDOW, entry->type);
  auto* window = static_cast<sessions::tab_restore::Window*>(entry);
  ASSERT_EQ(1u, window->tabs.size());
  EXPECT_EQ(group, window->tabs[0]->group);
  EXPECT_EQ(group_visual_data, window->tab_groups[group]->visual_data);
}

// Ensures tab extra data is restored from previous session.
TEST_F(TabRestoreServiceImplTest, TabExtraDataRestoredFromSessionData) {
  const char kSampleKey[] = "test";
  const char kSampleData[] = "true";

  CreateSessionServiceWithOneWindow(false);
  AddWindowWithOneTabToSessionService(false);

  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile());
  session_service->AddTabExtraData(window_id(), tab_id(), kSampleKey,
                                   kSampleData);

  SessionServiceFactory::GetForProfile(profile())
      ->MoveCurrentSessionToLastSession();
  EXPECT_FALSE(service_->IsLoaded());
  SynchronousLoadTabsFromLastSession();

  ASSERT_EQ(2U, service_->entries().size());
  Entry* entry = service_->entries().back().get();
  ASSERT_EQ(sessions::tab_restore::Type::WINDOW, entry->type);
  auto* window = static_cast<sessions::tab_restore::Window*>(entry);
  ASSERT_EQ(1U, window->tabs.size());
  ASSERT_EQ(1U, window->tabs[0]->extra_data.size());
  EXPECT_EQ(kSampleData, window->tabs[0]->extra_data[kSampleKey]);
}
