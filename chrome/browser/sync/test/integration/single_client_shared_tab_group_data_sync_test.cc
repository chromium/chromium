// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/saved_tab_groups_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace tab_groups {
namespace {

using testing::UnorderedElementsAre;

MATCHER_P3(HasSharedGroupMetadata, title, color, collaboration_id, "") {
  return base::UTF16ToUTF8(arg.title()) == title && arg.color() == color &&
         arg.collaboration_id() == collaboration_id;
}

MATCHER_P2(HasTabMetadata, title, url, "") {
  return base::UTF16ToUTF8(arg.title()) == title && arg.url() == GURL(url);
}

sync_pb::SharedTabGroupDataSpecifics MakeSharedTabGroupSpecifics(
    const base::Uuid& guid,
    const std::string& title,
    sync_pb::SharedTabGroup::Color color) {
  sync_pb::SharedTabGroupDataSpecifics specifics;
  specifics.set_guid(guid.AsLowercaseString());
  sync_pb::SharedTabGroup* pb_group = specifics.mutable_tab_group();
  pb_group->set_title(title);
  pb_group->set_color(color);
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

class SingleClientSharedTabGroupDataSyncTest : public SyncTest {
 public:
  SingleClientSharedTabGroupDataSyncTest() : SyncTest(SINGLE_CLIENT) {
    feature_overrides_.InitAndEnableFeature(
        data_sharing::features::kDataSharingFeature);
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
                /*client_tag=*/entity_specifics.shared_tab_group_data().guid(),
                entity_specifics, /*creation_time=*/0, /*last_modified_time=*/0,
                collaboration_id));
  }

  tab_groups::TabGroupSyncService* GetTabGroupSyncService() const {
    return tab_groups::TabGroupSyncServiceFactory::GetForProfile(GetProfile(0));
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
      MakeSharedTabGroupSpecifics(group_guid, "title",
                                  sync_pb::SharedTabGroup_Color_CYAN),
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

  TabGroupSyncService* tab_group_sync_service = GetTabGroupSyncService();
  EXPECT_THAT(
      tab_group_sync_service->GetAllGroups(),
      UnorderedElementsAre(HasSharedGroupMetadata(
          "title", tab_groups::TabGroupColorId::kCyan, collaboration_id)));
  ASSERT_TRUE(tab_group_sync_service->GetGroup(group_guid).has_value());
  EXPECT_THAT(
      tab_group_sync_service->GetGroup(group_guid)->saved_tabs(),
      UnorderedElementsAre(HasTabMetadata("tab 1", "http://google.com/1"),
                           HasTabMetadata("tab 2", "http://google.com/2")));
}

}  // namespace
}  // namespace tab_groups
