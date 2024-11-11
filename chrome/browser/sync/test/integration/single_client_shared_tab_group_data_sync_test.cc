// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/sync/test/integration/saved_tab_groups_helper.h"
#include "chrome/browser/sync/test/integration/shared_tab_group_data_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/sync_test_tab_utils.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/tab_groups/tab_group_color.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace tab_groups {
namespace {

constexpr char kDefaultContent[] =
    "<html><title>Title</title><body></body></html>";
constexpr char kDefaultURLPath[] = "/sync/simple.html";
constexpr char kDefaultTabTitle[] = "Title";

using testing::Contains;
using testing::SizeIs;
using testing::UnorderedElementsAre;

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == kDefaultURLPath) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_content_type("text/html");
    http_response->set_content(kDefaultContent);
    return http_response;
  }
  return nullptr;
}

sync_pb::SharedTabGroupDataSpecifics MakeSharedTabGroupSpecifics(
    const base::Uuid& guid,
    const base::Uuid& originating_saved_group_guid,
    const std::string& title,
    sync_pb::SharedTabGroup::Color color) {
  sync_pb::SharedTabGroupDataSpecifics specifics;
  specifics.set_guid(guid.AsLowercaseString());
  sync_pb::SharedTabGroup* pb_group = specifics.mutable_tab_group();
  pb_group->set_title(title);
  pb_group->set_color(color);
  pb_group->set_originating_tab_group_guid(
      originating_saved_group_guid.AsLowercaseString());
  return specifics;
}

sync_pb::SharedTabGroupDataSpecifics MakeSharedTabGroupTabSpecifics(
    const base::Uuid& guid,
    const base::Uuid& group_guid,
    const std::string& title,
    const GURL& url) {
  sync_pb::SharedTabGroupDataSpecifics specifics;
  specifics.set_guid(guid.AsLowercaseString());
  sync_pb::SharedTab* pb_tab = specifics.mutable_tab();
  pb_tab->set_title(title);
  pb_tab->set_shared_tab_group_guid(group_guid.AsLowercaseString());
  pb_tab->set_url(url.spec());
  return specifics;
}

std::string GetClientTag(const sync_pb::SharedTabGroupDataSpecifics& specifics,
                         const std::string& collaboration_id) {
  return specifics.guid() + "|" + collaboration_id;
}

class SingleClientSharedTabGroupDataSyncTest : public SyncTest {
 public:
  SingleClientSharedTabGroupDataSyncTest() : SyncTest(SINGLE_CLIENT) {
    feature_overrides_.InitWithFeatures(
        {data_sharing::features::kDataSharingFeature,
         tab_groups::kTabGroupsSaveUIUpdate, tab_groups::kTabGroupsSaveV2,
         tab_groups::kTabGroupSyncServiceDesktopMigration},
        {});
  }
  ~SingleClientSharedTabGroupDataSyncTest() override = default;

  void AddSpecificsToFakeServer(
      sync_pb::SharedTabGroupDataSpecifics shared_specifics,
      const std::string& collaboration_id) {
    // First, create the collaboration for the user.
    GetFakeServer()->AddCollaboration(collaboration_id);

    sync_pb::EntitySpecifics entity_specifics;
    *entity_specifics.mutable_shared_tab_group_data() =
        std::move(shared_specifics);
    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::
            CreateFromSharedSpecificsForTesting(
                /*non_unique_name=*/"",
                GetClientTag(entity_specifics.shared_tab_group_data(),
                             collaboration_id),
                entity_specifics, /*creation_time=*/0, /*last_modified_time=*/0,
                collaboration_id));
  }

  TabGroupSyncService* GetTabGroupSyncService() const {
    return TabGroupSyncServiceFactory::GetForProfile(GetProfile(0));
  }

  // Returns both saved and shared tab groups.
  std::vector<SavedTabGroup> GetAllTabGroups() const {
    return GetTabGroupSyncService()->GetAllGroups();
  }

  void AddTabGroup(SavedTabGroup group) {
    GetTabGroupSyncService()->AddGroup(std::move(group));
  }

  void MakeTabGroupShared(const LocalTabGroupID& local_group_id,
                          std::string_view collaboration_id) {
    GetTabGroupSyncService()->MakeTabGroupShared(local_group_id,
                                                 collaboration_id);
  }

  // Returns the only saved tab group specifics from the fake server. The group
  // must exist and be the only one.
  sync_pb::SavedTabGroupSpecifics GetOnlySavedTabGroupSpecificsFromServer() {
    sync_pb::SavedTabGroupSpecifics result_specifics;
    for (const sync_pb::SyncEntity& entity :
         GetFakeServer()->GetSyncEntitiesByDataType(syncer::SAVED_TAB_GROUP)) {
      if (!entity.specifics().saved_tab_group().has_group()) {
        continue;
      }

      // Verify that there are no two group specifics on the server.
      CHECK(!result_specifics.has_group());
      result_specifics = entity.specifics().saved_tab_group();
    }

    // Verify that there is one group on the server.
    CHECK(result_specifics.has_group());
    return result_specifics;
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleRequest));
    ASSERT_TRUE(embedded_test_server()->Start());
    SyncTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_overrides_;
};

IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       ShouldInitializeDataType) {
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::SHARED_TAB_GROUP_DATA));
}

IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       ShouldDownloadGroupsAndTabsAtInitialSync) {
  const base::Uuid group_guid = base::Uuid::GenerateRandomV4();
  const std::string collaboration_id = "collaboration";

  AddSpecificsToFakeServer(
      MakeSharedTabGroupSpecifics(
          group_guid,
          /*originating_saved_group_guid=*/base::Uuid::GenerateRandomV4(),
          "title", sync_pb::SharedTabGroup_Color_CYAN),
      collaboration_id);
  AddSpecificsToFakeServer(
      MakeSharedTabGroupTabSpecifics(base::Uuid::GenerateRandomV4(), group_guid,
                                     "tab 1", GURL("http://google.com/1")),
      collaboration_id);
  AddSpecificsToFakeServer(
      MakeSharedTabGroupTabSpecifics(base::Uuid::GenerateRandomV4(), group_guid,
                                     "tab 2", GURL("http://google.com/2")),
      collaboration_id);

  ASSERT_TRUE(SetupSync());

  ASSERT_THAT(GetAllTabGroups(),
              UnorderedElementsAre(HasSharedGroupMetadata(
                  "title", TabGroupColorId::kCyan, collaboration_id)));
  EXPECT_THAT(
      GetAllTabGroups().front().saved_tabs(),
      UnorderedElementsAre(HasTabMetadata("tab 1", "http://google.com/1"),
                           HasTabMetadata("tab 2", "http://google.com/2")));
}

IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       ShouldTransitionSavedToSharedTabGroup) {
  const GURL kUrl = embedded_test_server()->GetURL(kDefaultURLPath);
  ASSERT_TRUE(SetupSync());

  // Create a new group with a single tab, and wait until a new saved tab group
  // is committed to the server.
  std::optional<size_t> tab_index = sync_test_tab_utils::OpenNewTab(kUrl);
  ASSERT_TRUE(tab_index.has_value());

  LocalTabGroupID local_group_id = sync_test_tab_utils::CreateGroupFromTab(
      tab_index.value(), "title", tab_groups::TabGroupColorId::kBlue);

  ASSERT_TRUE(
      ServerSavedTabGroupMatchChecker(
          UnorderedElementsAre(
              HasSpecificsSavedTabGroup(
                  "title", sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_BLUE),
              HasSpecificsSavedTab(kDefaultTabTitle, kUrl.spec())))
          .Wait());

  // Add the user to the collaboration before making any changes (to prevent
  // filtration of local entities on GetUpdates before Commit).
  GetFakeServer()->AddCollaboration("collaboration");

  // Transition the saved tab group to shared tab group.
  MakeTabGroupShared(local_group_id, "collaboration");

  // Saved tab group remains intact, hence verify only that the shared tab group
  // is committed.
  EXPECT_TRUE(
      ServerSharedTabGroupMatchChecker(
          UnorderedElementsAre(HasSpecificsSharedTabGroup(
                                   "title", sync_pb::SharedTabGroup::BLUE),
                               HasSpecificsSharedTab(kDefaultTabTitle, kUrl)))
          .Wait());

  std::vector<sync_pb::SyncEntity> server_entities_shared =
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::SHARED_TAB_GROUP_DATA);
  ASSERT_THAT(server_entities_shared, SizeIs(2));
  // Put the tab group first for simplicity.
  if (server_entities_shared[0].specifics().shared_tab_group_data().has_tab()) {
    server_entities_shared[0].Swap(&server_entities_shared[1]);
  }
  const sync_pb::SharedTabGroupDataSpecifics& shared_group_specifics =
      server_entities_shared[0].specifics().shared_tab_group_data();
  const sync_pb::SharedTabGroupDataSpecifics& shared_tab_specifics =
      server_entities_shared[1].specifics().shared_tab_group_data();

  std::vector<sync_pb::SyncEntity> server_entities_saved =
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::SAVED_TAB_GROUP);
  ASSERT_THAT(server_entities_saved, SizeIs(2));
  // Put the tab group first for simplicity.
  if (server_entities_saved[0].specifics().saved_tab_group().has_tab()) {
    server_entities_saved[0].Swap(&server_entities_saved[1]);
  }
  const sync_pb::SavedTabGroupSpecifics& saved_group_specifics =
      server_entities_saved[0].specifics().saved_tab_group();
  const sync_pb::SavedTabGroupSpecifics& saved_tab_specifics =
      server_entities_saved[1].specifics().saved_tab_group();

  // Verify that GUIDs are different.
  EXPECT_NE(shared_group_specifics.guid(), saved_group_specifics.guid());
  EXPECT_NE(shared_tab_specifics.guid(), saved_tab_specifics.guid());

  // Verify the originating group GUID.
  EXPECT_EQ(shared_group_specifics.tab_group().originating_tab_group_guid(),
            saved_group_specifics.guid());
}

IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       ShouldTransitionSavedToSharedGroupRemotely) {
  const GURL kUrl = embedded_test_server()->GetURL(kDefaultURLPath);
  const std::string kCollaborationId = "collaboration";
  ASSERT_TRUE(SetupSync());

  // Create a new group with a single tab, and wait until a new saved tab group
  // is committed to the server.
  std::optional<size_t> tab_index = sync_test_tab_utils::OpenNewTab(kUrl);
  ASSERT_TRUE(tab_index.has_value());

  LocalTabGroupID local_group_id = sync_test_tab_utils::CreateGroupFromTab(
      tab_index.value(), "title", tab_groups::TabGroupColorId::kBlue);

  ASSERT_TRUE(
      ServerSavedTabGroupMatchChecker(
          UnorderedElementsAre(
              HasSpecificsSavedTabGroup(
                  "title", sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_BLUE),
              HasSpecificsSavedTab(kDefaultTabTitle, kUrl.spec())))
          .Wait());

  // Add the user to the collaboration before making any changes (to prevent
  // filtration of local entities on GetUpdates before Commit).
  GetFakeServer()->AddCollaboration(kCollaborationId);

  std::vector<SavedTabGroup> local_groups =
      GetTabGroupSyncService()->GetAllGroups();
  ASSERT_THAT(local_groups, SizeIs(1));

  const SavedTabGroup& saved_local_group = local_groups.front();
  ASSERT_FALSE(saved_local_group.is_shared_tab_group());
  ASSERT_THAT(saved_local_group.saved_tabs(), SizeIs(1));

  // Simulate remote transition from saved to shared tab group (by creating a
  // corresponding shared tab group).
  const base::Uuid shared_group_guid = base::Uuid::GenerateRandomV4();
  const base::Uuid shared_tab_guid = base::Uuid::GenerateRandomV4();
  AddSpecificsToFakeServer(
      MakeSharedTabGroupSpecifics(
          shared_group_guid,
          /*originating_saved_group_guid=*/saved_local_group.saved_guid(),
          "title", sync_pb::SharedTabGroup::CYAN),
      kCollaborationId);
  AddSpecificsToFakeServer(MakeSharedTabGroupTabSpecifics(
                               /*guid=*/shared_tab_guid, shared_group_guid,
                               kDefaultTabTitle, GURL(kUrl)),
                           kCollaborationId);

  // Wait for the new group to propagate with all tabs.
  ASSERT_TRUE(
      SavedTabOrGroupExistsChecker(GetTabGroupSyncService(), shared_group_guid)
          .Wait());
  ASSERT_TRUE(
      SavedTabOrGroupExistsChecker(GetTabGroupSyncService(), shared_tab_guid)
          .Wait());

  // Verify now that the tab group in UI is connected with the new shared tab
  // group.
  ASSERT_TRUE(sync_test_tab_utils::IsTabGroupOpen(local_group_id));

  sync_test_tab_utils::UpdateTabGroupVisualData(
      local_group_id, "New Title", tab_groups::TabGroupColorId::kGrey);

  // The shared tab group should be updated on the server.
  EXPECT_TRUE(ServerSharedTabGroupMatchChecker(
                  Contains(HasSpecificsSharedTabGroup(
                      "New Title", sync_pb::SharedTabGroup::GREY)))
                  .Wait());
}

// Android doesn't support PRE_ tests.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       PRE_ShouldReloadDataOnBrowserRestart) {
  const base::Uuid group_guid = base::Uuid::GenerateRandomV4();
  const std::string collaboration_id = "collaboration";

  AddSpecificsToFakeServer(
      MakeSharedTabGroupSpecifics(
          group_guid,
          /*originating_saved_group_guid=*/base::Uuid::GenerateRandomV4(),
          "title", sync_pb::SharedTabGroup_Color_CYAN),
      collaboration_id);
  AddSpecificsToFakeServer(
      MakeSharedTabGroupTabSpecifics(base::Uuid::GenerateRandomV4(), group_guid,
                                     "tab 1", GURL("http://google.com/1")),
      collaboration_id);
  AddSpecificsToFakeServer(
      MakeSharedTabGroupTabSpecifics(base::Uuid::GenerateRandomV4(), group_guid,
                                     "tab 2", GURL("http://google.com/2")),
      collaboration_id);

  ASSERT_TRUE(SetupSync());
  ASSERT_THAT(GetAllTabGroups(), SizeIs(1));
}

IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       ShouldReloadDataOnBrowserRestart) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  ASSERT_THAT(GetAllTabGroups(), SizeIs(1));
  EXPECT_THAT(
      GetAllTabGroups().front().saved_tabs(),
      UnorderedElementsAre(HasTabMetadata("tab 1", "http://google.com/1"),
                           HasTabMetadata("tab 2", "http://google.com/2")));
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace tab_groups
