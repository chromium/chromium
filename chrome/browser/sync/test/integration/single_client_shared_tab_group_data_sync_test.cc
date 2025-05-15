// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/saved_tab_groups_helper.h"
#include "chrome/browser/sync/test/integration/shared_tab_group_data_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/sync_test_tab_utils.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/collaboration_finder.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/collaboration_id.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/tab_groups/tab_group_color.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace tab_groups {
namespace {

constexpr char kDefaultContent[] =
    "<html><title>Title</title><body></body></html>";
constexpr char kDefaultURLPath[] = "/sync/simple.html";
constexpr char kDefaultTabTitle[] = "Title";

using tab_groups::HasSavedGroupMetadata;
using testing::Contains;
using testing::ElementsAre;
using testing::Optional;
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

sync_pb::SavedTabGroupSpecifics MakeSavedTabGroupSpecifics(
    const base::Uuid& guid,
    const std::string& title,
    sync_pb::SavedTabGroup::SavedTabGroupColor color) {
  sync_pb::SavedTabGroupSpecifics specifics;
  specifics.set_guid(guid.AsLowercaseString());
  sync_pb::SavedTabGroup* pb_group = specifics.mutable_group();
  pb_group->set_title(title);
  pb_group->set_color(color);
  return specifics;
}

sync_pb::SavedTabGroupSpecifics MakeSavedTabGroupTabSpecifics(
    const base::Uuid& guid,
    const base::Uuid& group_guid,
    const std::string& title,
    const GURL& url) {
  sync_pb::SavedTabGroupSpecifics specifics;
  specifics.set_guid(guid.AsLowercaseString());
  sync_pb::SavedTabGroupTab* pb_tab = specifics.mutable_tab();
  pb_tab->set_title(title);
  pb_tab->set_group_guid(group_guid.AsLowercaseString());
  pb_tab->set_url(url.spec());
  return specifics;
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

// Waits until the tab group exists in the model regardless any filtration (e.g.
// due to empty groups or transitioning).
class TabGroupExistsChecker : public SingleClientStatusChangeChecker {
 public:
  TabGroupExistsChecker(base::Uuid group_id,
                        TabGroupSyncService* tab_group_sync_service,
                        syncer::SyncServiceImpl* sync_service)
      : SingleClientStatusChangeChecker(sync_service),
        group_id_(group_id),
        tab_group_sync_service_(tab_group_sync_service) {
    CHECK(tab_group_sync_service_);
  }

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for tab group to exist without filtration.";
    return tab_group_sync_service_->GetGroup(group_id_).has_value();
  }

 private:
  const base::Uuid group_id_;
  raw_ptr<TabGroupSyncService> tab_group_sync_service_;
};

// Waits until the data type has an error.
class SharedTabGroupDataErrorChecker : public SingleClientStatusChangeChecker {
 public:
  using SingleClientStatusChangeChecker::SingleClientStatusChangeChecker;

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for SharedTabGroupData data type error.";
    return service()->HasAnyModelErrorForTest({syncer::SHARED_TAB_GROUP_DATA});
  }
};

class SingleClientSharedTabGroupDataSyncTest : public SyncTest {
 public:
  SingleClientSharedTabGroupDataSyncTest() : SyncTest(SINGLE_CLIENT) {
    feature_overrides_.InitWithFeatures(
        {data_sharing::features::kDataSharingFeature,
         tab_groups::kTabGroupSyncServiceDesktopMigration},
        {});
  }
  ~SingleClientSharedTabGroupDataSyncTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_ANDROID)
    if (base::android::BuildInfo::GetInstance()->is_automotive()) {
      // TODO(crbug.com/399444939): Re-enable once automotive is supported.
      GTEST_SKIP() << "Test shouldn't run on automotive builders.";
    }
#endif
    SyncTest::SetUp();
  }

  void RegisterCollaboration(const syncer::CollaborationId& collaboration_id) {
    GetTabGroupSyncService()
        ->GetCollaborationFinderForTesting()
        ->SetCollaborationAvailableForTesting(collaboration_id);
  }

  GaiaId GetGaiaId() const {
    return GetClient(0)->GetGaiaIdForDefaultTestAccount();
  }

  sync_pb::SyncEntity::CollaborationMetadata MakeCollaborationMetadata(
      const std::string& collaboration_id) {
    sync_pb::SyncEntity::CollaborationMetadata collaboration_metadata;
    collaboration_metadata.set_collaboration_id(collaboration_id);
    collaboration_metadata.mutable_creation_attribution()
        ->set_obfuscated_gaia_id(GetGaiaId().ToString());
    collaboration_metadata.mutable_last_update_attribution()
        ->set_obfuscated_gaia_id(GetGaiaId().ToString());
    return collaboration_metadata;
  }

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
                MakeCollaborationMetadata(collaboration_id)));
  }

  void AddSavedSpecificsToFakeServer(
      sync_pb::SavedTabGroupSpecifics saved_specifics) {
    sync_pb::EntitySpecifics entity_specifics;
    *entity_specifics.mutable_saved_tab_group() = std::move(saved_specifics);
    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"",
            /*client_tag=*/entity_specifics.saved_tab_group().guid(),
            entity_specifics,
            /*creation_time=*/0, /*last_modified_time=*/0));
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
    // TODO(crbug.com/382557489): use the proper callback.
    GetTabGroupSyncService()->MakeTabGroupShared(
        local_group_id, collaboration_id,
        TabGroupSyncService::TabGroupSharingCallback());
  }

  void InjectTombstoneToFakeServer(
      const sync_pb::SharedTabGroupDataSpecifics& shared_group_specifics,
      const CollaborationId& collaboration_id) {
    const syncer::ClientTagHash shared_group_client_tag_hash =
        syncer::ClientTagHash::FromUnhashed(
            syncer::SHARED_TAB_GROUP_DATA,
            GetClientTag(shared_group_specifics, collaboration_id.value()));

    GetFakeServer()->InjectEntity(
        syncer::PersistentTombstoneEntity::CreateNewShared(
            syncer::LoopbackServerEntity::CreateId(
                syncer::SHARED_TAB_GROUP_DATA,
                shared_group_client_tag_hash.value()),
            shared_group_client_tag_hash.value(),
            MakeCollaborationMetadata(collaboration_id.value())));
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

  // SetupClients() must be called to get access to IdentityManager before
  // injecting entities to the fake server.
  ASSERT_TRUE(SetupClients());

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
  RegisterCollaboration(syncer::CollaborationId(collaboration_id));

  std::vector<SavedTabGroup> service_groups = GetAllTabGroups();
  ASSERT_THAT(service_groups,
              UnorderedElementsAre(HasSharedGroupMetadata(
                  "title", TabGroupColorId::kCyan, collaboration_id)));
  const SavedTabGroup& group = service_groups.front();
  EXPECT_FALSE(group.creation_time().is_null());
  EXPECT_THAT(
      group.saved_tabs(),
      UnorderedElementsAre(HasTabMetadata("tab 1", "http://google.com/1"),
                           HasTabMetadata("tab 2", "http://google.com/2")));
  for (const SavedTabGroupTab& tab : group.saved_tabs()) {
    EXPECT_FALSE(tab.creation_time().is_null());
  }
}

// Flaky on Android: crbug.com/403333571.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ShouldTransitionSavedToSharedTabGroup \
  DISABLED_ShouldTransitionSavedToSharedTabGroup
#else
#define MAYBE_ShouldTransitionSavedToSharedTabGroup \
  ShouldTransitionSavedToSharedTabGroup
#endif
IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       MAYBE_ShouldTransitionSavedToSharedTabGroup) {
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
  // is committed. Page title will be sanitized when convering a saved tab group
  // to a shared tab group, thus is may not match the original title.
  // TODO(crbug.com/374221675): test the case that title is from optimization
  // guide.
  EXPECT_TRUE(ServerSharedTabGroupMatchChecker(
                  UnorderedElementsAre(
                      HasSpecificsSharedTabGroup("title",
                                                 sync_pb::SharedTabGroup::BLUE),
                      HasSpecificsSharedTab(
                          base::UTF16ToUTF8(
                              tab_groups::GetTitleFromUrlForDisplay(kUrl)),
                          kUrl)))
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

// Flaky on Android: crbug.com/403333571.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ShouldTransitionSavedToSharedGroupRemotely \
  DISABLED_ShouldTransitionSavedToSharedGroupRemotely
#else
#define MAYBE_ShouldTransitionSavedToSharedGroupRemotely \
  ShouldTransitionSavedToSharedGroupRemotely
#endif
IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       MAYBE_ShouldTransitionSavedToSharedGroupRemotely) {
  const GURL kUrl = embedded_test_server()->GetURL(kDefaultURLPath);
  const std::string kCollaborationId = "collaboration";

  ASSERT_TRUE(SetupSync());
  RegisterCollaboration(syncer::CollaborationId(kCollaborationId));

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

IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       ShouldIgnoreOriginatingSavedGroupAfterTransition) {
  const syncer::CollaborationId kCollaborationId("collaboration");
  const GURL kUrl = embedded_test_server()->GetURL(kDefaultURLPath);

  // SetupClients() must be called to get access to IdentityManager before
  // injecting entities to the fake server.
  ASSERT_TRUE(SetupClients());

  // Create a shared tab group remotely to avoid having local originating saved
  // group.
  const base::Uuid kOriginatingSavedGroupGuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kSharedGroupGuid = base::Uuid::GenerateRandomV4();
  AddSpecificsToFakeServer(
      MakeSharedTabGroupSpecifics(
          kSharedGroupGuid,
          /*originating_saved_group_guid=*/kOriginatingSavedGroupGuid, "title",
          sync_pb::SharedTabGroup::CYAN),
      kCollaborationId.value());

  const base::Uuid kSharedTabGuid = base::Uuid::GenerateRandomV4();
  AddSpecificsToFakeServer(
      MakeSharedTabGroupTabSpecifics(
          /*guid=*/kSharedTabGuid, kSharedGroupGuid, kDefaultTabTitle, kUrl),
      kCollaborationId.value());

  ASSERT_TRUE(SetupSync());
  RegisterCollaboration(kCollaborationId);

  ASSERT_THAT(GetTabGroupSyncService()->GetAllGroups(),
              ElementsAre(HasSharedGroupMetadata(
                  "title", TabGroupColorId::kCyan, kCollaborationId.value())));

  // Add the originating saved tab group remotely.
  AddSavedSpecificsToFakeServer(MakeSavedTabGroupSpecifics(
      kOriginatingSavedGroupGuid, "title",
      sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_BLUE));
  AddSavedSpecificsToFakeServer(MakeSavedTabGroupTabSpecifics(
      /*guid=*/base::Uuid::GenerateRandomV4(), kOriginatingSavedGroupGuid,
      kDefaultTabTitle, kUrl));

  ASSERT_TRUE(TabGroupExistsChecker(kOriginatingSavedGroupGuid,
                                    GetTabGroupSyncService(), GetSyncService(0))
                  .Wait());

  // The shared tab group should remain intact and the only one.
  EXPECT_THAT(GetTabGroupSyncService()->GetAllGroups(),
              ElementsAre(HasSharedGroupMetadata(
                  "title", TabGroupColorId::kCyan, kCollaborationId.value())));
}

IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       ShouldIgnoreTabGroupWithSameGuid) {
  ASSERT_TRUE(SetupSync());

  const CollaborationId kCollaborationId("collaboration");
  const base::Uuid kGroupGuid = base::Uuid::GenerateRandomV4();

  // Add a saved tab group locally and simulate a remote creation of a shared
  // tab group with the same GUID.
  tab_groups::SavedTabGroup saved_group(u"Saved Title", TabGroupColorId::kGrey,
                                        /*urls=*/{}, /*position=*/0,
                                        kGroupGuid);
  tab_groups::SavedTabGroupTab tab_1(GURL("http://google.com/saved_1"),
                                     u"Saved tab 1", kGroupGuid,
                                     /*position=*/0);
  tab_groups::SavedTabGroupTab tab_2(GURL("http://google.com/saved_2"),
                                     u"Saved tab 2", kGroupGuid,
                                     /*position=*/1);
  saved_group.AddTabLocally(tab_1);
  saved_group.AddTabLocally(tab_2);
  GetTabGroupSyncService()->AddGroup(saved_group);

  AddSpecificsToFakeServer(
      MakeSharedTabGroupSpecifics(
          kGroupGuid,
          /*originating_saved_group_guid=*/base::Uuid::GenerateRandomV4(),
          "title", sync_pb::SharedTabGroup_Color_CYAN),
      kCollaborationId.value());
  AddSpecificsToFakeServer(
      MakeSharedTabGroupTabSpecifics(/*guid=*/base::Uuid::GenerateRandomV4(),
                                     kGroupGuid, "tab 1",
                                     GURL("http://google.com/1")),
      kCollaborationId.value());
  AddSpecificsToFakeServer(
      MakeSharedTabGroupTabSpecifics(/*guid=*/base::Uuid::GenerateRandomV4(),
                                     kGroupGuid, "tab 2",
                                     GURL("http://google.com/2")),
      kCollaborationId.value());

  ASSERT_TRUE(AwaitQuiescence());

  // Verify that the saved tab group is still present and no shared tab group
  // was created locally.
  ASSERT_THAT(GetAllTabGroups(), SizeIs(1));
  EXPECT_THAT(GetAllTabGroups().front(),
              HasSavedGroupMetadata(u"Saved Title", TabGroupColorId::kGrey));
  EXPECT_THAT(
      GetAllTabGroups().front().saved_tabs(),
      ElementsAre(HasTabMetadata("Saved tab 1", "http://google.com/saved_1"),
                  HasTabMetadata("Saved tab 2", "http://google.com/saved_2")));
}

IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       ShouldIgnoreTabUpdatesWithGuidOfSavedGroup) {
  ASSERT_TRUE(SetupSync());

  const CollaborationId kCollaborationId("collaboration");

  tab_groups::SavedTabGroup saved_group(u"Saved Title", TabGroupColorId::kGrey,
                                        /*urls=*/{}, /*position=*/0);
  tab_groups::SavedTabGroupTab tab_1(GURL("http://google.com/saved_1"),
                                     u"Saved tab 1", saved_group.saved_guid(),
                                     /*position=*/0);
  tab_groups::SavedTabGroupTab tab_2(GURL("http://google.com/saved_2"),
                                     u"Saved tab 2", saved_group.saved_guid(),
                                     /*position=*/1);
  saved_group.AddTabLocally(tab_1);
  saved_group.AddTabLocally(tab_2);
  GetTabGroupSyncService()->AddGroup(saved_group);

  // Simulate a remote update of a tab with the parent GUID of the saved group.
  AddSpecificsToFakeServer(
      MakeSharedTabGroupTabSpecifics(/*guid=*/base::Uuid::GenerateRandomV4(),
                                     saved_group.saved_guid(), "tab 1",
                                     GURL("http://google.com/1")),
      kCollaborationId.value());

  // The same but have even GUID collision of tabs.
  AddSpecificsToFakeServer(
      MakeSharedTabGroupTabSpecifics(/*guid=*/tab_2.saved_tab_guid(),
                                     saved_group.saved_guid(), "tab 2",
                                     GURL("http://google.com/2")),
      kCollaborationId.value());

  ASSERT_TRUE(AwaitQuiescence());

  // Verify that the saved tab group is still present and no shared tabs were
  // created / updated locally.
  ASSERT_THAT(GetAllTabGroups(), SizeIs(1));
  EXPECT_THAT(GetAllTabGroups().front(),
              HasSavedGroupMetadata(u"Saved Title", TabGroupColorId::kGrey));
  EXPECT_THAT(
      GetAllTabGroups().front().saved_tabs(),
      ElementsAre(HasTabMetadata("Saved tab 1", "http://google.com/saved_1"),
                  HasTabMetadata("Saved tab 2", "http://google.com/saved_2")));
}

// This test covers the following scenario for the device #2:
// 1. User shares a saved tab group from device #1.
// 2. Shared tab group is committed to the server.
// 3. Shared tab group is received by device #2, the originating saved tab group
//    is transitioned to shared and marked as hidden.
// 4. Sharing fails on device #1 and the shared tab group is deleted (uploading
//    tombstones).
// 5. Device #2 receives the tombstones and applies the deletion of the shared
//    tab group. The originating saved tab group should be restored.
IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       ShouldRestoreOriginatingSavedGroupOnShareFailure) {
  const GURL kUrl = embedded_test_server()->GetURL(kDefaultURLPath);
  const CollaborationId kCollaborationId("collaboration");

  ASSERT_TRUE(SetupClients());
  RegisterCollaboration(kCollaborationId);

  // Create both shared and oritinating saved tab groups remotely.
  const base::Uuid kOriginatingSavedGroupGuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kSharedGroupGuid = base::Uuid::GenerateRandomV4();

  const sync_pb::SharedTabGroupDataSpecifics shared_group_specifics =
      MakeSharedTabGroupSpecifics(
          kSharedGroupGuid,
          /*originating_saved_group_guid=*/kOriginatingSavedGroupGuid, "title",
          sync_pb::SharedTabGroup::CYAN);
  AddSpecificsToFakeServer(shared_group_specifics, kCollaborationId.value());
  AddSpecificsToFakeServer(MakeSharedTabGroupTabSpecifics(
                               /*guid=*/base::Uuid::GenerateRandomV4(),
                               kSharedGroupGuid, kDefaultTabTitle, kUrl),
                           kCollaborationId.value());
  AddSavedSpecificsToFakeServer(MakeSavedTabGroupSpecifics(
      kOriginatingSavedGroupGuid, "title",
      sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_BLUE));
  AddSavedSpecificsToFakeServer(MakeSavedTabGroupTabSpecifics(
      /*guid=*/base::Uuid::GenerateRandomV4(), kOriginatingSavedGroupGuid,
      kDefaultTabTitle, kUrl));

  // The initial merge should result in a shared tab group with a hidden
  // originating saved tab group.
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(
      SavedTabOrGroupExistsChecker(GetTabGroupSyncService(), kSharedGroupGuid)
          .Wait());

  // Only shared tab group is available from GetAllGroups().
  ASSERT_THAT(GetTabGroupSyncService()->GetAllGroups(),
              ElementsAre(HasSharedGroupMetadata(
                  "title", TabGroupColorId::kCyan, kCollaborationId.value())));

  // The originating saved tab group is hidden but still available.
  ASSERT_THAT(
      GetTabGroupSyncService()->GetGroup(kOriginatingSavedGroupGuid),
      Optional(HasSavedGroupMetadata(u"title", TabGroupColorId::kBlue)));

  // Simulate a failure of the sharing operation on the remote client which
  // resulted in a tombstone of the shared tab group.
  InjectTombstoneToFakeServer(shared_group_specifics, kCollaborationId);

  ASSERT_TRUE(SavedTabOrGroupDoesNotExistChecker(GetTabGroupSyncService(),
                                                 kSharedGroupGuid)
                  .Wait());

  // The originating saved tab group should be restored and available.
  EXPECT_THAT(
      GetTabGroupSyncService()->GetAllGroups(),
      ElementsAre(HasSavedGroupMetadata(u"title", TabGroupColorId::kBlue)));
}

IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       ShouldFailDataTypeForCrossCollaborationUpdates) {
  ASSERT_TRUE(SetupSync());

  const base::Uuid kGroupGuid = base::Uuid::GenerateRandomV4();
  const std::string kCollaborationId = "collaboration";
  RegisterCollaboration(syncer::CollaborationId(kCollaborationId));

  // Create 2 shared tab groups.
  AddSpecificsToFakeServer(
      MakeSharedTabGroupSpecifics(
          kGroupGuid,
          /*originating_saved_group_guid=*/base::Uuid::GenerateRandomV4(),
          "title", sync_pb::SharedTabGroup_Color_CYAN),
      kCollaborationId);
  AddSpecificsToFakeServer(
      MakeSharedTabGroupTabSpecifics(base::Uuid::GenerateRandomV4(), kGroupGuid,
                                     "tab 1", GURL("http://google.com/1")),
      kCollaborationId);

  ASSERT_TRUE(SavedTabOrGroupExistsChecker(GetTabGroupSyncService(), kGroupGuid)
                  .Wait());

  // Simulate an update of a tab but from another collaboration.
  AddSpecificsToFakeServer(
      MakeSharedTabGroupTabSpecifics(base::Uuid::GenerateRandomV4(), kGroupGuid,
                                     "tab 1", GURL("http://google.com/1")),
      "other_collaboration");

  // The data type is expected to fail.
  ExcludeDataTypesFromCheckForDataTypeFailures({syncer::SHARED_TAB_GROUP_DATA});

  EXPECT_TRUE(SharedTabGroupDataErrorChecker(GetSyncService(0)).Wait());
}

// Android doesn't support PRE_ tests.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       PRE_ShouldReloadDataOnBrowserRestart) {
  const base::Uuid group_guid = base::Uuid::GenerateRandomV4();
  const std::string collaboration_id = "collaboration";

  // SetupClients() must be called to get access to IdentityManager before
  // injecting entities to the fake server.
  ASSERT_TRUE(SetupClients());

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
  RegisterCollaboration(syncer::CollaborationId(collaboration_id));

  ASSERT_THAT(GetAllTabGroups(), SizeIs(1));
}

IN_PROC_BROWSER_TEST_F(SingleClientSharedTabGroupDataSyncTest,
                       ShouldReloadDataOnBrowserRestart) {
  const std::string collaboration_id = "collaboration";
  ASSERT_TRUE(SetupClients());
  GetFakeServer()->AddCollaboration(collaboration_id);
  RegisterCollaboration(syncer::CollaborationId(collaboration_id));
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  ASSERT_THAT(GetAllTabGroups(), SizeIs(1));
  EXPECT_THAT(
      GetAllTabGroups().front().saved_tabs(),
      UnorderedElementsAre(HasTabMetadata("tab 1", "http://google.com/1"),
                           HasTabMetadata("tab 2", "http://google.com/2")));
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace tab_groups
