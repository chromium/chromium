// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/suggestion_service_lacros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/test/test_future.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/crosapi/mojom/suggestion_service.mojom.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kSessionName1[] = "test_session_name 1";
constexpr char kSessionName2[] = "test_session_name 2";

constexpr char kSessionTag1[] = "SessionTag1";
constexpr char kSessionTag2[] = "SessionTag2";

constexpr char kExampleURL1[] = "http://www.example.com/1";
constexpr char kExampleURL2[] = "http://www.example.com/2";

constexpr char16_t kTabTitle1[] = u"Tab Title 1";
constexpr char16_t kTabTitle2[] = u"Tab Title 2";

std::unique_ptr<sync_sessions::SyncedSession> CreateNewSession(
    const std::string& session_name,
    const std::string& session_tag,
    syncer::DeviceInfo::FormFactor form_factor) {
  auto session = std::make_unique<sync_sessions::SyncedSession>();
  auto window = std::make_unique<sync_sessions::SyncedSessionWindow>();
  auto tab = std::make_unique<sessions::SessionTab>();

  session->SetSessionName(session_name);
  session->SetDeviceTypeAndFormFactor(sync_pb::SyncEnums::TYPE_UNSET,
                                      form_factor);

  window->wrapped_window.tabs.push_back(std::move(tab));
  session->windows[SessionID::NewUnique()] = std::move(window);
  session->SetSessionTag(session_tag);
  return session;
}

class MockSessionSyncService : public sync_sessions::SessionSyncService {
 public:
  MockSessionSyncService() = default;
  ~MockSessionSyncService() override = default;

  MOCK_METHOD(syncer::GlobalIdMapper*,
              GetGlobalIdMapper,
              (),
              (const, override));
  MOCK_METHOD(sync_sessions::OpenTabsUIDelegate*,
              GetOpenTabsUIDelegate,
              (),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              SubscribeToForeignSessionsChanged,
              (const base::RepeatingClosure& cb),
              (override));
  MOCK_METHOD(base::WeakPtr<syncer::ModelTypeControllerDelegate>,
              GetControllerDelegate,
              ());
};

class MockOpenTabsUIDelegate : public sync_sessions::OpenTabsUIDelegate {
 public:
  MockOpenTabsUIDelegate() {
    foreign_sessions_owned_.push_back(CreateNewSession(
        kSessionName1, kSessionTag1, syncer::DeviceInfo::FormFactor::kDesktop));
    foreign_sessions_.push_back(foreign_sessions_owned_.back().get());
    foreign_sessions_owned_.push_back(CreateNewSession(
        kSessionName2, kSessionTag2, syncer::DeviceInfo::FormFactor::kPhone));
    foreign_sessions_.push_back(foreign_sessions_owned_.back().get());

    std::vector<std::unique_ptr<sessions::SessionTab>> session_tabs_one;
    auto tab = std::make_unique<sessions::SessionTab>();
    tab->timestamp = base::Time::Now();
    tab->navigations.push_back(sessions::SerializedNavigationEntryTestHelper::
                                   CreateNavigationForTest());
    tab->navigations[0].set_timestamp(base::Time::Now());
    tab->navigations[0].set_title(kTabTitle1);
    tab->navigations[0].set_virtual_url(GURL(kExampleURL1));
    session_tabs_one.push_back(std::move(tab));

    std::vector<std::unique_ptr<sessions::SessionTab>> session_tabs_two;
    tab = std::make_unique<sessions::SessionTab>();
    tab->timestamp = base::Time::Now();
    tab->navigations.push_back(sessions::SerializedNavigationEntryTestHelper::
                                   CreateNavigationForTest());
    tab->navigations[0].set_timestamp(base::Time::Now() + base::Minutes(5));
    tab->navigations[0].set_title(kTabTitle2);
    tab->navigations[0].set_virtual_url(GURL(kExampleURL2));
    session_tabs_two.push_back(std::move(tab));

    session_tabs_.emplace(kSessionTag1, std::move(session_tabs_one));
    session_tabs_.emplace(kSessionTag2, std::move(session_tabs_two));
  }

  MOCK_METHOD(bool,
              GetForeignTab,
              (const std::string&,
               const SessionID,
               const sessions::SessionTab**));
  MOCK_METHOD(void, DeleteForeignSession, (const std::string& tag));
  MOCK_METHOD(std::vector<const sessions::SessionWindow*>,
              GetForeignSession,
              (const std::string&));
  MOCK_METHOD(bool, GetLocalSession, (const sync_sessions::SyncedSession**));

  bool GetAllForeignSessions(
      std::vector<raw_ptr<const sync_sessions::SyncedSession,
                          VectorExperimental>>* sessions) override {
    *sessions = foreign_sessions_;
    base::ranges::sort(*sessions, std::greater(),
                       [](const sync_sessions::SyncedSession* session) {
                         return session->GetModifiedTime();
                       });

    return !sessions->empty();
  }

  bool GetForeignSessionTabs(
      const std::string& tag,
      std::vector<const sessions::SessionTab*>* tabs) override {
    auto it = session_tabs_.find(tag);
    if (it != session_tabs_.end()) {
      for (auto& tab : it->second) {
        tabs->push_back(tab.get());
      }
    }
    return true;
  }

 private:
  std::vector<std::unique_ptr<sync_sessions::SyncedSession>>
      foreign_sessions_owned_;
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      foreign_sessions_;
  std::map<std::string, std::vector<std::unique_ptr<sessions::SessionTab>>>
      session_tabs_;
};

std::unique_ptr<KeyedService> BuildMockSessionSyncService(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<MockSessionSyncService>>();
}

}  // namespace

class SuggestionServiceLacrosTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    auto* session_sync_service = static_cast<MockSessionSyncService*>(
        SessionSyncServiceFactory::GetInstance()->GetForProfile(GetProfile()));
    EXPECT_CALL(*session_sync_service, GetOpenTabsUIDelegate())
        .WillRepeatedly(testing::Return(&open_tabs_delegate_));
  }

  void SetTestingFactory(content::BrowserContext* context) {
    SessionSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        context, base::BindRepeating([](::content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<MockSessionSyncService>();
        }));
  }

  TestingProfile* CreateProfile(const std::string& profile_name) override {
    return profile_manager()->CreateTestingProfile(profile_name,
                                                   GetTestingFactories(), true);
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {
        {SessionSyncServiceFactory::GetInstance(),
         base::BindRepeating(&BuildMockSessionSyncService)},
    };
  }

 private:
  MockOpenTabsUIDelegate open_tabs_delegate_;
};

// Test that SuggestionServiceLacros gets tabs properly.
TEST_F(SuggestionServiceLacrosTest, GetTabSuggestions) {
  SuggestionServiceLacros suggestion_service_lacros;

  base::test::TestFuture<std::vector<crosapi::mojom::TabSuggestionItemPtr>>
      future;

  suggestion_service_lacros.GetTabSuggestionItems(future.GetCallback());

  const auto tabs = future.Take();

  ASSERT_EQ(tabs.size(), 2u);

  EXPECT_EQ(tabs[0]->title, base::UTF16ToUTF8(kTabTitle1));
  EXPECT_EQ(tabs[0]->url, GURL(kExampleURL1));
  EXPECT_EQ(tabs[0]->session_name, kSessionName1);
  EXPECT_EQ(tabs[0]->form_factor,
            crosapi::mojom::SuggestionDeviceFormFactor::kDesktop);

  EXPECT_EQ(tabs[1]->title, base::UTF16ToUTF8(kTabTitle2));
  EXPECT_EQ(tabs[1]->url, GURL(kExampleURL2));
  EXPECT_EQ(tabs[1]->session_name, kSessionName2);
  EXPECT_EQ(tabs[1]->form_factor,
            crosapi::mojom::SuggestionDeviceFormFactor::kPhone);
}
