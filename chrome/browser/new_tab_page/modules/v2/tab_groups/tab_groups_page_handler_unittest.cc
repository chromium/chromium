// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups_page_handler.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups.mojom.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/public/versioning_message_controller.h"
#include "components/search/ntp_features.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
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
              AddUrl,
              (const base::Uuid&, const std::u16string&, const GURL&));
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
              (const syncer::CollaborationId&),
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

namespace {

class MockDeviceInfoTracker : public syncer::DeviceInfoTracker {
 public:
  MOCK_METHOD(bool, IsSyncing, (), (const));
  MOCK_METHOD(syncer::DeviceInfo*,
              GetDeviceInfo,
              (const std::string& client_id),
              (const));
  MOCK_METHOD(std::vector<const syncer::DeviceInfo*>,
              GetAllDeviceInfo,
              (),
              (const));
  MOCK_METHOD(std::vector<const syncer::DeviceInfo*>,
              GetAllChromeDeviceInfo,
              (),
              (const));
  MOCK_METHOD(void, AddObserver, (Observer * observer));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer));
  MOCK_METHOD((absl::flat_hash_map<syncer::DeviceInfo::FormFactor, int>),
              CountActiveDevicesByType,
              (),
              (const));
  MOCK_METHOD(void, ForcePulseForTest, ());
  MOCK_METHOD(bool,
              IsRecentLocalCacheGuid,
              (const std::string& cache_guid),
              (const));
};

class MockDeviceInfoSyncService : public syncer::DeviceInfoSyncService {
 public:
  MOCK_METHOD(syncer::LocalDeviceInfoProvider*, GetLocalDeviceInfoProvider, ());
  MOCK_METHOD(syncer::DeviceInfoTracker*, GetDeviceInfoTracker, ());
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetControllerDelegate,
              ());
  MOCK_METHOD(void, RefreshLocalDeviceInfo, ());
};

}  // namespace

class TabGroupsPageHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  TabGroupsPageHandlerTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Setup mock TabGroupSyncService.
    auto* tab_group_sync_service_factory =
        tab_groups::TabGroupSyncServiceFactory::GetInstance();
    tab_group_sync_service_factory->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              testing::NiceMock<tab_groups::MockTabGroupSyncService>>();
        }));
    mock_service_ = static_cast<tab_groups::MockTabGroupSyncService*>(
        tab_group_sync_service_factory->GetForProfile(profile()));

    // Setup mock_device_info_sync_service_.
    auto* device_info_sync_service_factory =
        DeviceInfoSyncServiceFactory::GetInstance();
    device_info_sync_service_factory->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              testing::NiceMock<MockDeviceInfoSyncService>>();
        }));
    mock_device_info_sync_service_ = static_cast<MockDeviceInfoSyncService*>(
        device_info_sync_service_factory->GetForProfile(profile()));

    // Setup mock_device_info_tracker_.
    mock_device_info_tracker_ =
        std::make_unique<testing::NiceMock<MockDeviceInfoTracker>>();
    ON_CALL(*mock_device_info_sync_service_, GetDeviceInfoTracker())
        .WillByDefault(testing::Return(mock_device_info_tracker_.get()));

    webui::SetBrowserWindowInterface(web_contents(),
                                     &mock_browser_window_interface_);
    test_tab_strip_model_delegate_.SetBrowserWindowInterface(
        &mock_browser_window_interface_);
    tab_strip_model_ = std::make_unique<TabStripModel>(
        &test_tab_strip_model_delegate_, profile());
    ON_CALL(mock_browser_window_interface_, GetTabStripModel())
        .WillByDefault(testing::Return(tab_strip_model_.get()));

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
    mock_device_info_sync_service_ = nullptr;
    mock_device_info_tracker_.reset();
    handler_.reset();
    tab_strip_model_.reset();
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
           TabGroupsOptional tab_groups_arg, bool show_zero_state) {
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
    tab_groups::SavedTabGroup group1(u"Third Group",
                                     tab_groups::TabGroupColorId::kGrey,
                                     std::move(tabs1), 0);
    group1.SetCollaborationId(syncer::CollaborationId("collaboration_id"));
    groups.emplace_back(group1);
    groups.back().SetUpdateTime(base::Time::Now() -
                                base::Days(8));  // Used 1 week ago

    std::vector<tab_groups::SavedTabGroupTab> tabs2;
    tabs2.emplace_back(GURL("https://b.com/"), u"Title B",
                       base::Uuid::GenerateRandomV4(), 0);
    tabs2.emplace_back(GURL("https://c.com/"), u"Title C",
                       base::Uuid::GenerateRandomV4(), 0);
    tab_groups::SavedTabGroup group2(u"Second Group",
                                     tab_groups::TabGroupColorId::kGreen,
                                     std::move(tabs2), 1);
    group2.SetCollaborationId(syncer::CollaborationId("collaboration_id"));
    groups.emplace_back(group2);
    groups.back().SetUpdateTime(base::Time::Now() -
                                base::Hours(25));  // Used 1 day ago

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
    groups.emplace_back(u"Newest Group", tab_groups::TabGroupColorId::kBlue,
                        std::move(tabs3), 2);
    groups.back().SetUpdateTime(base::Time::Now() -
                                base::Hours(1));  // Recently used

    std::vector<tab_groups::SavedTabGroupTab> tabs4;
    tabs4.emplace_back(GURL("https://i.com/"), u"Title I",
                       base::Uuid::GenerateRandomV4(), 0);
    groups.emplace_back(u"Fourth Group", tab_groups::TabGroupColorId::kBlue,
                        std::move(tabs4), 3);
    groups.back().SetUpdateTime(base::Time::Now() -
                                base::Days(20));  // Used 2 weeks ago

    std::vector<tab_groups::SavedTabGroupTab> tabs5;
    tabs5.emplace_back(GURL("https://j.com/"), u"Title J",
                       base::Uuid::GenerateRandomV4(), 0);
    groups.emplace_back(u"Fifth Group", tab_groups::TabGroupColorId::kBlue,
                        std::move(tabs5), 4);
    groups.back().SetUpdateTime(base::Time::Now() -
                                base::Days(99));  // Used 14 weeks ago

    return groups;
  }

  std::unique_ptr<syncer::DeviceInfo> BuildDeviceInfo(std::string cache_guid,
                                                      std::string device_name) {
    return std::make_unique<syncer::DeviceInfo>(
        cache_guid, device_name, "chrome_version", "user_agent",
        sync_pb::SyncEnums::TYPE_UNSET, syncer::DeviceInfo::OsType::kUnknown,
        syncer::DeviceInfo::FormFactor::kUnknown, "device_id",
        "manufacturer_name", "model_name", "full_hardware_class",
        base::Time::Now(), base::Minutes(60),
        /*send_tab_to_self_receiving_enabled=*/false,
        /*send_tab_to_self_receiving_type=*/
        sync_pb::
            SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
        /*sharing_info=*/std::nullopt, /*paask_info=*/std::nullopt,
        "fcm_registration_token", /*interested_data_types=*/
        Difference(syncer::ProtocolTypes(), syncer::CommitOnlyTypes()),
        /*auto_sign_out_last_signin_timestamp=*/std::nullopt);
  }

  tab_groups::MockTabGroupSyncService* service() { return mock_service_; }
  TabGroupsPageHandler* handler() { return handler_.get(); }
  std::vector<const tab_groups::SavedTabGroup*> saved_tab_groups() {
    return saved_tab_groups_;
  }
  MockDeviceInfoTracker* mock_device_info_tracker() {
    return mock_device_info_tracker_.get();
  }
  TabStripModel* tab_strip_model() { return tab_strip_model_.get(); }

 private:
  raw_ptr<tab_groups::MockTabGroupSyncService> mock_service_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  TestTabStripModelDelegate test_tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  mojo::PendingRemote<ntp::tab_groups::mojom::PageHandler> page_handler_remote_;
  std::unique_ptr<TabGroupsPageHandler> handler_;

  std::vector<tab_groups::SavedTabGroup> owned_groups_;
  std::vector<const tab_groups::SavedTabGroup*> saved_tab_groups_;

  raw_ptr<MockDeviceInfoSyncService> mock_device_info_sync_service_;
  std::unique_ptr<MockDeviceInfoTracker> mock_device_info_tracker_;
};

TEST_F(TabGroupsPageHandlerTest, GetSavedTabGroups_WithDeviceInfo) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpTabGroupsModule, {});
  const std::string kCacheGuid = "test_cache_guid";
  const std::string kDeviceName = "TestDevice";
  const std::string kUpdateTime = "Recently used";

  // Create a tab group with a specific cache_guid.
  std::vector<tab_groups::SavedTabGroup> groups;
  groups.emplace_back(u"Group Title", tab_groups::TabGroupColorId::kGrey,
                      std::vector<tab_groups::SavedTabGroupTab>{});
  groups.back().SetUpdateTime(base::Time::Now());
  groups.back().SetLastUpdaterCacheGuid(kCacheGuid);
  std::vector<const tab_groups::SavedTabGroup*> groups_ptr;
  groups_ptr.push_back(&groups[0]);

  // Set up the mock tracker to return a specific DeviceInfo for the given guid.
  auto device_info = BuildDeviceInfo(kCacheGuid, kDeviceName);
  ON_CALL(*mock_device_info_tracker(), GetDeviceInfo(kCacheGuid))
      .WillByDefault(testing::Return(device_info.get()));
  EXPECT_CALL(*service(), ReadAllGroups())
      .WillRepeatedly(testing::Return(groups_ptr));

  auto result = RunGetTabGroups();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(1u, result->size());
  EXPECT_EQ(kUpdateTime, result.value()[0]->update_time);
  EXPECT_EQ(kDeviceName, result.value()[0]->device_name);
}

TEST_F(TabGroupsPageHandlerTest, GetSavedTabGroups_DeviceInfoNotFound) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpTabGroupsModule, {});

  // Create a group with a cache_guid that cannot be found.
  const std::string kUnknownCacheGuid = "unknown_guid";
  std::vector<tab_groups::SavedTabGroup> groups;
  groups.emplace_back(u"Group Title", tab_groups::TabGroupColorId::kGrey,
                      std::vector<tab_groups::SavedTabGroupTab>{});
  groups.back().SetUpdateTime(base::Time::Now());
  groups.back().SetLastUpdaterCacheGuid(kUnknownCacheGuid);
  std::vector<const tab_groups::SavedTabGroup*> groups_ptr;
  groups_ptr.push_back(&groups[0]);

  // Set up mock tracker to return nullptr for the unknown guid.
  ON_CALL(*mock_device_info_tracker(), GetDeviceInfo(kUnknownCacheGuid))
      .WillByDefault(testing::Return(nullptr));
  EXPECT_CALL(*service(), ReadAllGroups())
      .WillRepeatedly(testing::Return(groups_ptr));

  auto result = RunGetTabGroups();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(1u, result->size());
  EXPECT_EQ("Recently used", result.value()[0]->update_time);
  EXPECT_EQ(std::nullopt, result.value()[0]->device_name);
}

TEST_F(TabGroupsPageHandlerTest, GetSavedTabGroups_NoCacheGuid) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpTabGroupsModule, {});

  // Create tab groups data.
  std::vector<tab_groups::SavedTabGroup> groups;
  groups.emplace_back(u"Group Title", tab_groups::TabGroupColorId::kGrey,
                      std::vector<tab_groups::SavedTabGroupTab>{});
  groups.back().SetUpdateTime(base::Time::Now());
  std::vector<const tab_groups::SavedTabGroup*> groups_ptr;
  groups_ptr.push_back(&groups[0]);

  EXPECT_CALL(*mock_device_info_tracker(), GetDeviceInfo(testing::_)).Times(0);
  EXPECT_CALL(*service(), ReadAllGroups())
      .WillRepeatedly(testing::Return(groups_ptr));

  auto result = RunGetTabGroups();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(1u, result->size());
  EXPECT_EQ("Recently used", result.value()[0]->update_time);
  EXPECT_EQ(std::nullopt, result.value()[0]->device_name);
}

TEST_F(TabGroupsPageHandlerTest,
       GetSavedTabGroups_DoNotReturnDeviceNameForCurrentDevice) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpTabGroupsModule, {});

  // Create a tab group with a specific cache GUID.
  const std::string kLocalCacheGuid = "local_device_guid";
  std::vector<tab_groups::SavedTabGroup> groups;
  groups.emplace_back(u"Group Title", tab_groups::TabGroupColorId::kGrey,
                      std::vector<tab_groups::SavedTabGroupTab>{});
  groups.back().SetUpdateTime(base::Time::Now());
  groups.back().SetLastUpdaterCacheGuid(kLocalCacheGuid);

  std::vector<const tab_groups::SavedTabGroup*> groups_ptr;
  groups_ptr.push_back(&groups[0]);

  ON_CALL(*mock_device_info_tracker(), IsRecentLocalCacheGuid(kLocalCacheGuid))
      .WillByDefault(testing::Return(true));
  EXPECT_CALL(*mock_device_info_tracker(), GetDeviceInfo(kLocalCacheGuid))
      .Times(0);
  EXPECT_CALL(*service(), ReadAllGroups())
      .WillRepeatedly(testing::Return(groups_ptr));

  auto result = RunGetTabGroups();

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(1u, result->size());
  EXPECT_EQ(std::nullopt, result.value()[0]->device_name);
}

TEST_F(TabGroupsPageHandlerTest, GetSavedTabGroups_Empty) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpTabGroupsModule, {});

  EXPECT_CALL(*service(), ReadAllGroups())
      .WillRepeatedly(
          testing::Return(std::vector<const tab_groups::SavedTabGroup*>{}));

  auto result = RunGetTabGroups();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST_F(TabGroupsPageHandlerTest,
       GetSavedTabGroups_UseDefaultTitleForUnnamedGroups) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpTabGroupsModule, {});

  std::vector<tab_groups::SavedTabGroup> groups;

  // Create a group with no title and 1 tab.
  std::vector<tab_groups::SavedTabGroupTab> tabs1;
  tabs1.emplace_back(GURL("https://a.com/"), u"Tab 1",
                     base::Uuid::GenerateRandomV4(), 0);
  groups.emplace_back(/*title=*/u"", tab_groups::TabGroupColorId::kGrey,
                      std::move(tabs1), 0);
  groups.back().SetUpdateTime(base::Time::Now());

  // Create a group with no title and 2 tabs.
  std::vector<tab_groups::SavedTabGroupTab> tabs2;
  tabs2.emplace_back(GURL("https://example.com/1"), u"Tab 1",
                     base::Uuid::GenerateRandomV4(), 0);
  tabs2.emplace_back(GURL("https://example.com/2"), u"Tab 2",
                     base::Uuid::GenerateRandomV4(), 1);
  groups.emplace_back(/*title=*/u"", tab_groups::TabGroupColorId::kBlue,
                      std::move(tabs2), 0);
  groups.back().SetUpdateTime(base::Time::Now() - base::Hours(1));

  std::vector<const tab_groups::SavedTabGroup*> groups_ptr;
  for (const auto& group : groups) {
    groups_ptr.push_back(&group);
  }

  EXPECT_CALL(*service(), ReadAllGroups())
      .WillRepeatedly(testing::Return(groups_ptr));

  auto result = RunGetTabGroups();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(2u, result->size());
  // IDS_SAVED_TAB_GROUP_TABS_COUNT is translated to "Tab(s)" on MacOS, and
  // "tab(s)" elsewhere.
  EXPECT_EQ("1 tab", base::ToLowerASCII(result.value()[0]->title));
  EXPECT_EQ("2 tabs", base::ToLowerASCII(result.value()[1]->title));
}

TEST_F(TabGroupsPageHandlerTest, GetSavedTabGroups) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpTabGroupsModule,
      {{ntp_features::kNtpTabGroupsModuleMaxGroupCountParam.name, "4"}});

  EXPECT_CALL(*service(), ReadAllGroups())
      .WillRepeatedly(testing::Return(saved_tab_groups()));

  auto groups = RunGetTabGroups();
  ASSERT_TRUE(groups.has_value());

  const auto& group = groups.value();
  ASSERT_EQ(4u, group.size());

  const auto& group1 = group[0];
  EXPECT_EQ(tab_groups::TabGroupColorId::kBlue, group1->color);
  EXPECT_EQ("Newest Group", group1->title);
  EXPECT_EQ("Recently used", group1->update_time);
  EXPECT_EQ(5, group1->total_tab_count);
  ASSERT_EQ(4u, group1->favicon_urls.size());
  EXPECT_EQ(GURL("https://d.com/"), group1->favicon_urls[0]);
  EXPECT_EQ(GURL("https://e.com/"), group1->favicon_urls[1]);
  EXPECT_EQ(GURL("https://f.com/"), group1->favicon_urls[2]);
  EXPECT_EQ(GURL("https://g.com/"), group1->favicon_urls[3]);
  EXPECT_EQ(false, group1->is_shared_tab_group);

  const auto& group2 = group[1];
  EXPECT_EQ(tab_groups::TabGroupColorId::kGreen, group2->color);
  EXPECT_EQ("Second Group", group2->title);
  EXPECT_EQ("Used 1 day ago", group2->update_time);
  EXPECT_EQ(2, group2->total_tab_count);
  ASSERT_EQ(2u, group2->favicon_urls.size());
  EXPECT_EQ(GURL("https://b.com/"), group2->favicon_urls[0]);
  EXPECT_EQ(GURL("https://c.com/"), group2->favicon_urls[1]);
  EXPECT_EQ(true, group2->is_shared_tab_group);

  const auto& group3 = group[2];
  EXPECT_EQ(tab_groups::TabGroupColorId::kGrey, group3->color);
  EXPECT_EQ("Third Group", group3->title);
  EXPECT_EQ("Used 1 week ago", group3->update_time);
  EXPECT_EQ(true, group3->is_shared_tab_group);

  const auto& group4 = group[3];
  EXPECT_EQ(tab_groups::TabGroupColorId::kBlue, group4->color);
  EXPECT_EQ("Fourth Group", group4->title);
  EXPECT_EQ("Used 2 weeks ago", group4->update_time);
  EXPECT_EQ(false, group4->is_shared_tab_group);
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
  EXPECT_EQ(tab_groups::TabGroupColorId::kBlue, group1->color);
  EXPECT_EQ("Tab Group 1 (3 tabs total)", group1->title);
  EXPECT_EQ(3, group1->total_tab_count);
  EXPECT_EQ(3u, group1->favicon_urls.size());
  EXPECT_EQ(group1->total_tab_count,
            static_cast<int>(group1->favicon_urls.size()));
  EXPECT_EQ(GURL("https://www.google.com"), group1->favicon_urls[0]);
  EXPECT_EQ(GURL("https://www.google.com"), group1->favicon_urls[1]);
  EXPECT_EQ(GURL("https://www.google.com"), group1->favicon_urls[2]);
  EXPECT_EQ("Recently used", group1->update_time);
  EXPECT_EQ("Test Device", group1->device_name);
  EXPECT_EQ(false, group1->is_shared_tab_group);

  const auto& group2 = tab_groups[1];
  EXPECT_EQ(tab_groups::TabGroupColorId::kPurple, group2->color);
  EXPECT_EQ("Tab Group 2 (4 tabs total)", group2->title);
  EXPECT_EQ(4u, group2->favicon_urls.size());
  EXPECT_EQ(4, group2->total_tab_count);
  EXPECT_EQ("Used 1 day ago", group2->update_time);
  EXPECT_EQ("Test Device", group2->device_name);
  EXPECT_EQ(true, group2->is_shared_tab_group);

  const auto& group3 = tab_groups[2];
  EXPECT_EQ(tab_groups::TabGroupColorId::kYellow, group3->color);
  EXPECT_EQ("Tab Group 3 (8 tabs total)", group3->title);
  EXPECT_EQ(4u, group3->favicon_urls.size());
  EXPECT_EQ(8, group3->total_tab_count);
  EXPECT_EQ("Used 1 week ago", group3->update_time);
  EXPECT_EQ("Test Device", group3->device_name);
  EXPECT_EQ(false, group3->is_shared_tab_group);

  const auto& group4 = tab_groups[3];
  EXPECT_EQ(tab_groups::TabGroupColorId::kGreen, group4->color);
  EXPECT_EQ("Tab Group 4 (199 tabs total)", group4->title);
  EXPECT_EQ(4u, group4->favicon_urls.size());
  EXPECT_EQ(199, group4->total_tab_count);
  EXPECT_EQ("Used 2 weeks ago", group4->update_time);
  EXPECT_EQ(std::nullopt, group4->device_name);
  EXPECT_EQ(true, group4->is_shared_tab_group);
}

TEST_F(TabGroupsPageHandlerTest, GetFakeZeroStateTabGroups) {
  // Enable the feature and set the parameter to "Fake Zero State".
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpTabGroupsModule,
      {{ntp_features::kNtpTabGroupsModuleDataParam, "Fake Zero State"}});

  // Call GetTabGroups() with the future's callback.
  base::test::TestFuture<TabGroupsOptional, bool> future;
  handler()->GetTabGroups(future.GetCallback());

  const auto& [tab_groups_mojom, should_show_zero_state] = future.Get();
  ASSERT_TRUE(tab_groups_mojom.has_value());
  EXPECT_TRUE(tab_groups_mojom->empty());
  EXPECT_TRUE(should_show_zero_state);
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
