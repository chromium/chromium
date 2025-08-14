// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups_page_handler.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups.mojom.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/public/versioning_message_controller.h"
#include "components/search/ntp_features.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using TabGroupsOptional =
    std::optional<std::vector<ntp::tab_groups::mojom::TabGroupPtr>>;

namespace tab_groups {

// This is a test-only mock as it cannot include TabGroupSyncDelegate legally.
class TabGroupSyncDelegate {
 public:
  virtual ~TabGroupSyncDelegate() = default;
};

class MockTabGroupSyncService : public TabGroupSyncService {
 public:
  MockTabGroupSyncService();
  ~MockTabGroupSyncService() override;

  MOCK_METHOD(void,
              SetTabGroupSyncDelegate,
              (std::unique_ptr<TabGroupSyncDelegate>));
  MOCK_METHOD(void, AddGroup, (SavedTabGroup));
  MOCK_METHOD(void, RemoveGroup, (const LocalTabGroupID&));
  MOCK_METHOD(void, RemoveGroup, (const base::Uuid&));
  MOCK_METHOD(void,
              UpdateVisualData,
              (const LocalTabGroupID, const tab_groups::TabGroupVisualData*));
  MOCK_METHOD(void,
              UpdateGroupPosition,
              (const base::Uuid& sync_id,
               std::optional<bool> is_pinned,
               std ::optional<int> new_index));
  MOCK_METHOD(void,
              UpdateBookmarkNodeId,
              (const base::Uuid&, std::optional<base::Uuid>));
  MOCK_METHOD(void,
              AddTab,
              (const LocalTabGroupID&,
               const LocalTabID&,
               const std::u16string&,
               const GURL&,
               std::optional<size_t>));
  MOCK_METHOD(void,
              NavigateTab,
              (const LocalTabGroupID&,
               const LocalTabID&,
               const GURL&,
               const std::u16string&));
  MOCK_METHOD(void,
              UpdateTabProperties,
              (const LocalTabGroupID&,
               const LocalTabID&,
               const SavedTabGroupTabBuilder&));
  MOCK_METHOD(void, RemoveTab, (const LocalTabGroupID&, const LocalTabID&));
  MOCK_METHOD(void, MoveTab, (const LocalTabGroupID&, const LocalTabID&, int));
  MOCK_METHOD(void,
              OnTabSelected,
              (const std::optional<LocalTabGroupID>&,
               const LocalTabID&,
               const std::u16string&));
  MOCK_METHOD(void, SaveGroup, (SavedTabGroup));
  MOCK_METHOD(void, UnsaveGroup, (const LocalTabGroupID&));
  MOCK_METHOD(void,
              MakeTabGroupShared,
              (const LocalTabGroupID&,
               const syncer::CollaborationId&,
               TabGroupSharingCallback));
  MOCK_METHOD(void,
              MakeTabGroupSharedForTesting,
              (const LocalTabGroupID&, const syncer::CollaborationId&));
  MOCK_METHOD(void, MakeTabGroupUnsharedForTesting, (const LocalTabGroupID&));
  MOCK_METHOD(void,
              AboutToUnShareTabGroup,
              (const LocalTabGroupID&, base::OnceClosure));
  MOCK_METHOD(void, OnTabGroupUnShareComplete, (const LocalTabGroupID&, bool));
  MOCK_METHOD(void, OnCollaborationRemoved, (const syncer::CollaborationId&));

  MOCK_METHOD(std::vector<const SavedTabGroup*>, ReadAllGroups, (), (const));
  MOCK_METHOD(std::vector<SavedTabGroup>, GetAllGroups, (), (const));
  MOCK_METHOD(std::optional<SavedTabGroup>,
              GetGroup,
              (const base::Uuid&),
              (const));
  MOCK_METHOD(std::optional<SavedTabGroup>,
              GetGroup,
              (const LocalTabGroupID&),
              (const));
  MOCK_METHOD(std::optional<SavedTabGroup>,
              GetGroup,
              (const EitherGroupID&),
              (const));
  MOCK_METHOD(std::vector<LocalTabGroupID>, GetDeletedGroupIds, (), (const));
  MOCK_METHOD(std::optional<std::u16string>,
              GetTitleForPreviouslyExistingSharedTabGroup,
              (const CollaborationId&),
              (const));

  MOCK_METHOD(std::optional<LocalTabGroupID>,
              OpenTabGroup,
              (const base::Uuid&, std::unique_ptr<TabGroupActionContext>));
  MOCK_METHOD(void,
              UpdateLocalTabGroupMapping,
              (const base::Uuid&, const LocalTabGroupID&, OpeningSource));
  MOCK_METHOD(void,
              RemoveLocalTabGroupMapping,
              (const LocalTabGroupID&, ClosingSource));
  MOCK_METHOD(void,
              UpdateLocalTabId,
              (const LocalTabGroupID&, const base::Uuid&, const LocalTabID&));
  MOCK_METHOD(void,
              ConnectLocalTabGroup,
              (const base::Uuid&, const LocalTabGroupID&, OpeningSource));
  MOCK_METHOD(bool,
              IsRemoteDevice,
              (const std::optional<std::string>&),
              (const));
  MOCK_METHOD(bool,
              WasTabGroupClosedLocally,
              (const base::Uuid& sync_id),
              (const));
  MOCK_METHOD(void, RecordTabGroupEvent, (const EventDetails&));
  MOCK_METHOD(void, UpdateArchivalStatus, (const base::Uuid&, bool));
  MOCK_METHOD(void,
              UpdateTabLastSeenTime,
              (const base::Uuid&, const base::Uuid&, TriggerSource));
  MOCK_METHOD(TabGroupSyncMetricsLogger*, GetTabGroupSyncMetricsLogger, ());

  MOCK_METHOD(syncer::DataTypeSyncBridge*, bridge, ());
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetSavedTabGroupControllerDelegate,
              ());
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetSharedTabGroupControllerDelegate,
              ());
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetSharedTabGroupAccountControllerDelegate,
              ());
  MOCK_METHOD(std::unique_ptr<ScopedLocalObservationPauser>,
              CreateScopedLocalObserverPauser,
              ());
  MOCK_METHOD(void,
              GetURLRestriction,
              (const GURL&, TabGroupSyncService::UrlRestrictionCallback));
  MOCK_METHOD(std::unique_ptr<std::vector<SavedTabGroup>>,
              TakeSharedTabGroupsAvailableAtStartupForMessaging,
              ());
  MOCK_METHOD(bool, HadSharedTabGroupsLastSession, (bool), (override));
  MOCK_METHOD(VersioningMessageController*,
              GetVersioningMessageController,
              (),
              (override));
  MOCK_METHOD(void, OnLastTabClosed, (const SavedTabGroup&));

  MOCK_METHOD(void, AddObserver, (Observer*));
  MOCK_METHOD(void, RemoveObserver, (Observer*));
};

}  // namespace tab_groups

class TabGroupsPageHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  TabGroupsPageHandlerTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Setup mock TabGroupSyncService.
    auto* factory = tab_groups::TabGroupSyncServiceFactory::GetInstance();
    factory->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              testing::NiceMock<tab_groups::MockTabGroupSyncService>>();
        }));
    mock_service_ = static_cast<tab_groups::MockTabGroupSyncService*>(
        factory->GetForProfile(profile()));

    handler_ = std::make_unique<TabGroupsPageHandler>(
        mojo::PendingReceiver<ntp::tab_groups::mojom::PageHandler>(),
        web_contents());

    // Setup tab groups data.
    owned_groups_ = BuildTabGroups();
    for (const auto& group : owned_groups_) {
      saved_tab_groups_.push_back(&group);
    }
  }

  void TearDown() override {
    owned_groups_.clear();
    saved_tab_groups_.clear();
    handler_.reset();
    mock_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Synchronously fetches tab groups data from
  // `TabGroupsPageHandler::GetTabGroups()`. The actual mojo call is async, and
  // this helper blocks the current thread until the page handler responds to
  // achieve synchronization.
  TabGroupsOptional RunGetTabGroups() {
    TabGroupsOptional tab_groups_mojom;
    base::RunLoop wait_loop;
    handler_->GetTabGroups(base::BindOnce(
        [](base::OnceClosure stop_waiting, TabGroupsOptional* tab_groups,
           TabGroupsOptional tab_groups_arg) {
          *tab_groups = std::move(tab_groups_arg);
          std::move(stop_waiting).Run();
        },
        wait_loop.QuitClosure(), &tab_groups_mojom));
    wait_loop.Run();
    return tab_groups_mojom;
  }

  std::vector<tab_groups::SavedTabGroup> BuildTabGroups() {
    std::vector<tab_groups::SavedTabGroup> groups;

    std::vector<tab_groups::SavedTabGroupTab> tabs1;
    tabs1.emplace_back(GURL("https://a.com/"), u"Title A",
                       base::Uuid::GenerateRandomV4(), 0);
    groups.emplace_back(u"Old Group", tab_groups::TabGroupColorId::kGrey,
                        std::move(tabs1), 0);
    groups.back().SetUpdateTime(base::Time::Now() - base::Hours(3));

    std::vector<tab_groups::SavedTabGroupTab> tabs2;
    tabs2.emplace_back(GURL("https://b.com/"), u"Title B",
                       base::Uuid::GenerateRandomV4(), 0);
    tabs2.emplace_back(GURL("https://c.com/"), u"Title C",
                       base::Uuid::GenerateRandomV4(), 0);
    groups.emplace_back(u"Middle Group", tab_groups::TabGroupColorId::kGreen,
                        std::move(tabs2), 1);
    groups.back().SetUpdateTime(base::Time::Now() - base::Hours(2));

    std::vector<tab_groups::SavedTabGroupTab> tabs3;
    tabs3.emplace_back(GURL("https://d.com/"), u"Title D",
                       base::Uuid::GenerateRandomV4(), 0);
    tabs3.emplace_back(GURL("https://e.com/"), u"Title E",
                       base::Uuid::GenerateRandomV4(), 0);
    tabs3.emplace_back(GURL("https://f.com/"), u"Title F",
                       base::Uuid::GenerateRandomV4(), 0);
    tabs3.emplace_back(GURL("https://g.com/"), u"Title G",
                       base::Uuid::GenerateRandomV4(), 0);
    tabs3.emplace_back(GURL("https://h.com/"), u"Title H",
                       base::Uuid::GenerateRandomV4(), 0);
    groups.emplace_back(u"New Group", tab_groups::TabGroupColorId::kBlue,
                        std::move(tabs3), 2);
    groups.back().SetUpdateTime(base::Time::Now() - base::Hours(1));

    return groups;
  }

  tab_groups::MockTabGroupSyncService* service() { return mock_service_; }
  TabGroupsPageHandler* handler() { return handler_.get(); }
  std::vector<const tab_groups::SavedTabGroup*> saved_tab_groups() {
    return saved_tab_groups_;
  }

 private:
  raw_ptr<tab_groups::MockTabGroupSyncService> mock_service_;
  mojo::PendingRemote<ntp::tab_groups::mojom::PageHandler> page_handler_remote_;
  std::unique_ptr<TabGroupsPageHandler> handler_;

  std::vector<tab_groups::SavedTabGroup> owned_groups_;
  std::vector<const tab_groups::SavedTabGroup*> saved_tab_groups_;
};

TEST_F(TabGroupsPageHandlerTest, GetSavedTabGroups_Empty) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpTabGroupsModule, {});

  EXPECT_CALL(*service(), ReadAllGroups())
      .WillOnce(
          testing::Return(std::vector<const tab_groups::SavedTabGroup*>{}));

  auto result = RunGetTabGroups();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST_F(TabGroupsPageHandlerTest, GetSavedTabGroups) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpTabGroupsModule,
      {{ntp_features::kNtpTabGroupsModuleMaxGroupCountParam.name, "2"}});

  EXPECT_CALL(*service(), ReadAllGroups())
      .WillOnce(testing::Return(saved_tab_groups()));

  auto groups = RunGetTabGroups();
  ASSERT_TRUE(groups.has_value());

  const auto& group = groups.value();
  ASSERT_EQ(2u, group.size());

  const auto& group1 = group[0];
  EXPECT_EQ("New Group", group1->title);
  EXPECT_EQ(5, group1->total_tab_count);
  ASSERT_EQ(4u, group1->favicon_urls.size());
  EXPECT_EQ(GURL("https://d.com/"), group1->favicon_urls[0]);
  EXPECT_EQ(GURL("https://e.com/"), group1->favicon_urls[1]);
  EXPECT_EQ(GURL("https://f.com/"), group1->favicon_urls[2]);
  EXPECT_EQ(GURL("https://g.com/"), group1->favicon_urls[3]);

  const auto& group2 = group[1];
  EXPECT_EQ("Middle Group", group2->title);
  EXPECT_EQ(2, group2->total_tab_count);
  ASSERT_EQ(2u, group2->favicon_urls.size());
  EXPECT_EQ(GURL("https://b.com/"), group2->favicon_urls[0]);
  EXPECT_EQ(GURL("https://c.com/"), group2->favicon_urls[1]);
}

TEST_F(TabGroupsPageHandlerTest, GetFakeTabGroups) {
  // Enable the feature and set the parameter to "Fake Data".
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpTabGroupsModule,
      {{ntp_features::kNtpTabGroupsModuleDataParam, "Fake Data"}});

  auto tab_groups_mojom = RunGetTabGroups();
  ASSERT_TRUE(tab_groups_mojom.has_value());

  const auto& tab_groups = tab_groups_mojom.value();
  ASSERT_FALSE(tab_groups.empty());

  const auto& group1 = tab_groups[0];
  EXPECT_EQ("Tab Group 1 (3 tabs total)", group1->title);
  EXPECT_EQ(3, group1->total_tab_count);
  EXPECT_EQ(3u, group1->favicon_urls.size());
  EXPECT_EQ(group1->total_tab_count,
            static_cast<int>(group1->favicon_urls.size()));
  EXPECT_EQ(GURL("https://www.google.com"), group1->favicon_urls[0]);
  EXPECT_EQ(GURL("https://www.youtube.com"), group1->favicon_urls[1]);
  EXPECT_EQ(GURL("https://www.wikipedia.org"), group1->favicon_urls[2]);

  const auto& group2 = tab_groups[1];
  EXPECT_EQ("Tab Group 2 (4 tabs total)", group2->title);
  EXPECT_EQ(4u, group2->favicon_urls.size());
  EXPECT_EQ(4, group2->total_tab_count);

  const auto& group3 = tab_groups[2];
  EXPECT_EQ("Tab Group 3 (8 tabs total)", group3->title);
  EXPECT_EQ(4u, group3->favicon_urls.size());
  EXPECT_EQ(8, group3->total_tab_count);

  const auto& group4 = tab_groups[3];
  EXPECT_EQ("Tab Group 4 (199 tabs total)", group4->title);
  EXPECT_EQ(4u, group4->favicon_urls.size());
  EXPECT_EQ(199, group4->total_tab_count);
}

TEST_F(TabGroupsPageHandlerTest, GetFakeZeroStateTabGroups) {
  // Enable the feature and set the parameter to "Fake Zero State".
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpTabGroupsModule,
      {{ntp_features::kNtpTabGroupsModuleDataParam, "Fake Zero State"}});

  auto tab_groups_mojom = RunGetTabGroups();
  ASSERT_TRUE(tab_groups_mojom.has_value());

  const auto& tab_groups = tab_groups_mojom.value();
  EXPECT_TRUE(tab_groups.empty());
}

TEST_F(TabGroupsPageHandlerTest, DismissAndRestoreModule) {
  // Enable the feature and set the parameter to "Fake Data".
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpTabGroupsModule,
      {{ntp_features::kNtpTabGroupsModuleDataParam, "Fake Data"}});

  // With no dismissal pref set we should get the fake data.
  auto initial_tab_groups = RunGetTabGroups();
  ASSERT_TRUE(initial_tab_groups.has_value());
  EXPECT_FALSE(initial_tab_groups.value().empty());

  // Call DismissModule() and subsequent GetTabGroups() must return nullopt.
  handler()->DismissModule();
  auto module_dismissed = RunGetTabGroups();
  EXPECT_FALSE(module_dismissed.has_value());

  // Call RestoreModule() and data should again be returned.
  handler()->RestoreModule();
  auto module_restored = RunGetTabGroups();
  ASSERT_TRUE(module_restored.has_value());
  EXPECT_FALSE(module_restored.value().empty());
}
