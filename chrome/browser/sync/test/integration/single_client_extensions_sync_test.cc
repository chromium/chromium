// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/browser/sync/test/integration/await_match_status_change_checker.h"
#include "chrome/browser/sync/test/integration/extensions_helper.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server.h"
#include "content/public/test/browser_test.h"

namespace {

using extensions_helper::DisableExtension;
using extensions_helper::GetInstalledExtensions;
using extensions_helper::InstallExtension;
using extensions_helper::InstallExtensionForAllProfiles;

std::ostream& operator<<(std::ostream& os,
                         const base::flat_set<std::string>& set) {
  os << "{";
  for (const std::string& element : set) {
    os << element << ", ";
  }
  os << "}";
  return os;
}

// Checks if server extension IDs match the IDs provided in the constructor.
class TestServerExtensionIds
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  explicit TestServerExtensionIds(base::flat_set<std::string> extension_ids)
      : expected_extension_ids_(extension_ids) {}

  // FakeServerMatchStatusChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override {
    std::vector<sync_pb::SyncEntity> entities =
        fake_server()->GetSyncEntitiesByDataType(syncer::EXTENSIONS);
    base::flat_set<std::string> actual_extension_ids =
        base::MakeFlatSet<std::string>(
            entities,
            /*comp=*/{}, /*proj=*/[](const sync_pb::SyncEntity& e) {
              return e.specifics().extension().id();
            });
    *os << "Expected ids in fake server: " << expected_extension_ids_
        << ". Actual ids " << actual_extension_ids << ".";
    return expected_extension_ids_ == actual_extension_ids;
  }

 private:
  base::flat_set<std::string> expected_extension_ids_;
};

class SingleClientExtensionsSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientExtensionsSyncTest() : SyncTest(SINGLE_CLIENT) {
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      scoped_feature_list_.InitAndEnableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    }
  }
  ~SingleClientExtensionsSyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  // TODO(https://crbug.com/40804030): Remove when these tests use only MV3
  // extensions.
  extensions::ScopedTestMV2Enabler mv2_enabler_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientExtensionsSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(SingleClientExtensionsSyncTest, StartWithNoExtensions) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(TestServerExtensionIds({}).Wait());
}

IN_PROC_BROWSER_TEST_P(SingleClientExtensionsSyncTest,
                       StartWithSomeExtensions) {
  ASSERT_TRUE(SetupClients());

  std::string id0 = InstallExtension(GetProfile(0), 0);
  std::string id1 = InstallExtension(GetProfile(0), 1);

  ASSERT_TRUE(SetupSync());

  // Add one more extension after enabling sync, to ensure there's something to
  // wait for.
  std::string id2 = InstallExtension(GetProfile(0), 2);

  if (GetSetupSyncMode() == SyncTest::SetupSyncMode::kSyncTransportOnly) {
    // Only the extension that was added after syncing was turned on should
    // arrive on the server.
    EXPECT_TRUE(TestServerExtensionIds({id2}).Wait());
  } else {
    EXPECT_TRUE(TestServerExtensionIds({id0, id1, id2}).Wait());
  }
}

IN_PROC_BROWSER_TEST_P(SingleClientExtensionsSyncTest, InstallSomeExtensions) {
  ASSERT_TRUE(SetupSync());

  std::string id0 = InstallExtension(GetProfile(0), 0);
  std::string id1 = InstallExtension(GetProfile(0), 1);
  std::string id2 = InstallExtension(GetProfile(0), 2);

  ASSERT_TRUE(TestServerExtensionIds({id0, id1, id2}).Wait());
}

// Helper function for waiting to see the extension count in a profile
// become a specific number.
static bool ExtensionCountCheck(Profile* profile,
                                size_t expected_count,
                                std::ostream* os) {
  const size_t actual_count = GetInstalledExtensions(profile).size();
  *os << "Waiting for profile to have " << expected_count
      << " extensions; actual count " << actual_count;
  return actual_count == expected_count;
}

// Tests the case of an uninstall from the server conflicting with a local
// modification, which we expect to be resolved in favor of the uninstall.
IN_PROC_BROWSER_TEST_P(SingleClientExtensionsSyncTest, UninstallWinsConflicts) {
  ASSERT_TRUE(SetupClients());

  ASSERT_TRUE(SetupSync());
  std::string id0 = InstallExtension(GetProfile(0), 0);
  ASSERT_TRUE(TestServerExtensionIds({id0}).Wait());

  // Simulate a delete at the server.
  std::vector<sync_pb::SyncEntity> server_extensions =
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::EXTENSIONS);
  ASSERT_EQ(1ul, server_extensions.size());
  std::unique_ptr<syncer::LoopbackServerEntity> tombstone(
      syncer::PersistentTombstoneEntity::CreateNew(
          server_extensions[0].id_string(),
          server_extensions[0].client_tag_hash()));
  GetFakeServer()->InjectEntity(std::move(tombstone));

  // Modify the extension in the local profile to cause a conflict.
  DisableExtension(GetProfile(0), 0);
  ASSERT_EQ(1u, GetInstalledExtensions(GetProfile(0)).size());

  // Expect the extension to get uninstalled locally.
  AwaitMatchStatusChangeChecker checker(base::BindRepeating(
      &ExtensionCountCheck, GetProfile(0), /*expected_count=*/0u));
  EXPECT_TRUE(checker.Wait());
  EXPECT_TRUE(GetInstalledExtensions(GetProfile(0)).empty());

  // Expect the extension to remain uninstalled at the server.
  server_extensions =
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::EXTENSIONS);
  EXPECT_EQ(0ul, server_extensions.size());
}

}  // namespace
