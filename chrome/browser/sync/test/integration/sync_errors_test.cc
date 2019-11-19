// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/common/pref_names.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/protocol/sync_protocol_error.h"
#include "google_apis/gaia/google_service_auth_error.h"

using bookmarks::BookmarkNode;
using bookmarks_helper::AddFolder;
using bookmarks_helper::SetTitle;
using syncer::ProfileSyncService;

namespace {

class SyncDisabledChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncDisabledChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  SyncDisabledChecker(ProfileSyncService* service,
                      base::OnceClosure condition_satisfied_callback)
      : SingleClientStatusChangeChecker(service),
        condition_satisfied_callback_(std::move(condition_satisfied_callback)) {
  }

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting until sync is disabled";
    bool satisfied = !service()->IsSetupInProgress() &&
                     !service()->GetUserSettings()->IsFirstSetupComplete();
    if (satisfied && condition_satisfied_callback_) {
      std::move(condition_satisfied_callback_).Run();
    }
    return satisfied;
  }

 private:
  base::OnceClosure condition_satisfied_callback_;
};

class SyncEngineStoppedChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncEngineStoppedChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for sync to stop";
    return !service()->IsEngineInitialized();
  }
};

class TypeDisabledChecker : public SingleClientStatusChangeChecker {
 public:
  explicit TypeDisabledChecker(ProfileSyncService* service,
                               syncer::ModelType type)
      : SingleClientStatusChangeChecker(service), type_(type) {}

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for type " << syncer::ModelTypeToString(type_)
        << " to become disabled";
    return !service()->GetActiveDataTypes().Has(type_);
  }

 private:
  syncer::ModelType type_;
};

class SyncErrorTest : public SyncTest {
 public:
  SyncErrorTest() : SyncTest(SINGLE_CLIENT) {}
  ~SyncErrorTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncErrorTest);
};

// Helper class that waits until the sync engine has hit an actionable error.
class ActionableErrorChecker : public SingleClientStatusChangeChecker {
 public:
  explicit ActionableErrorChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  ~ActionableErrorChecker() override {}

  // Checks if an actionable error has been hit. Called repeatedly each time PSS
  // notifies observers of a state change.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting until sync hits an actionable error";
    syncer::SyncStatus status;
    service()->QueryDetailedSyncStatusForDebugging(&status);
    return (status.sync_protocol_error.action != syncer::UNKNOWN_ACTION &&
            service()->HasUnrecoverableError());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ActionableErrorChecker);
};

IN_PROC_BROWSER_TEST_F(SyncErrorTest, BirthdayErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add an item, wait for sync, and trigger a birthday error on the server.
  const BookmarkNode* node1 = AddFolder(0, 0, "title1");
  SetTitle(0, node1, "new_title1");
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  GetFakeServer()->ClearServerData();

  // Now make one more change so we will do another sync.
  const BookmarkNode* node2 = AddFolder(0, 0, "title2");
  SetTitle(0, node2, "new_title2");
  ASSERT_TRUE(SyncDisabledChecker(GetSyncService(0)).Wait());
}

IN_PROC_BROWSER_TEST_F(SyncErrorTest, ActionableErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* node1 = AddFolder(0, 0, "title1");
  SetTitle(0, node1, "new_title1");
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  std::string description = "Not My Fault";
  std::string url = "www.google.com";
  GetFakeServer()->TriggerActionableError(sync_pb::SyncEnums::TRANSIENT_ERROR,
                                          description, url,
                                          sync_pb::SyncEnums::UPGRADE_CLIENT);

  // Now make one more change so we will do another sync.
  const BookmarkNode* node2 = AddFolder(0, 0, "title2");
  SetTitle(0, node2, "new_title2");

  // Wait until an actionable error is encountered.
  ASSERT_TRUE(ActionableErrorChecker(GetSyncService(0)).Wait());

  syncer::SyncStatus status;
  GetSyncService(0)->QueryDetailedSyncStatusForDebugging(&status);
  ASSERT_EQ(status.sync_protocol_error.error_type, syncer::TRANSIENT_ERROR);
  ASSERT_EQ(status.sync_protocol_error.action, syncer::UPGRADE_CLIENT);
  ASSERT_EQ(status.sync_protocol_error.url, url);
  ASSERT_EQ(status.sync_protocol_error.error_description, description);
}

// This test verifies that sync keeps retrying if it encounters error during
// setup.
// crbug.com/689662
#if defined(OS_CHROMEOS)
#define MAYBE_ErrorWhileSettingUp DISABLED_ErrorWhileSettingUp
#else
#define MAYBE_ErrorWhileSettingUp ErrorWhileSettingUp
#endif
IN_PROC_BROWSER_TEST_F(SyncErrorTest, MAYBE_ErrorWhileSettingUp) {
  ASSERT_TRUE(SetupClients());

#if !defined(OS_CHROMEOS)
  // On non auto start enabled environments if the setup sync fails then
  // the setup would fail. So setup sync normally.
  // In contrast on auto start enabled platforms like chrome os we should be
  // able to set up even if the first sync while setting up fails.
  ASSERT_TRUE(SetupSync()) << "Setup sync failed";
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kAutofill));
#endif

  GetFakeServer()->TriggerError(sync_pb::SyncEnums::TRANSIENT_ERROR);
  EXPECT_TRUE(GetFakeServer()->EnableAlternatingTriggeredErrors());

#if defined(OS_CHROMEOS)
  // Now setup sync and it should succeed.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
#else
  // Now enable a datatype, whose first 2 syncs would fail, but we should
  // recover and setup succesfully on the third attempt.
  ASSERT_TRUE(
      GetClient(0)->EnableSyncForType(syncer::UserSelectableType::kAutofill));
#endif
}

IN_PROC_BROWSER_TEST_F(SyncErrorTest, BirthdayErrorUsingActionableErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* node1 = AddFolder(0, 0, "title1");
  SetTitle(0, node1, "new_title1");
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // Clear the server data so that the birthday gets incremented, and also send
  // an appropriate error.
  GetFakeServer()->ClearServerData();
  GetFakeServer()->TriggerActionableError(sync_pb::SyncEnums::NOT_MY_BIRTHDAY,
                                          "Not My Fault", "www.google.com",
                                          sync_pb::SyncEnums::UNKNOWN_ACTION);

  // Now make one more change so we will do another sync.
  const BookmarkNode* node2 = AddFolder(0, 0, "title2");
  SetTitle(0, node2, "new_title2");

  auto condition = base::BindLambdaForTesting([&]() {
    syncer::SyncStatus status;
    GetSyncService(0)->QueryDetailedSyncStatusForDebugging(&status);

    // Note: If SyncStandaloneTransport is enabled, then on receiving the error,
    // the SyncService will immediately start up again in transport mode, which
    // resets the status. So query the status that the checker recorded at the
    // time Sync was off.
    EXPECT_EQ(status.sync_protocol_error.error_type, syncer::NOT_MY_BIRTHDAY);
    EXPECT_EQ(status.sync_protocol_error.action,
              syncer::DISABLE_SYNC_ON_CLIENT);
  });
  EXPECT_TRUE(SyncDisabledChecker(GetSyncService(0), condition).Wait());
}

// Tests that on receiving CLIENT_DATA_OBSOLETE sync engine gets restarted and
// initialized with different cache_guid.
IN_PROC_BROWSER_TEST_F(SyncErrorTest, ClientDataObsoleteTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* node1 = AddFolder(0, 0, "title1");
  SetTitle(0, node1, "new_title1");
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  std::string description = "Not My Fault";
  std::string url = "www.google.com";

  // Remember cache_guid before actionable error.
  syncer::SyncStatus status;
  GetSyncService(0)->QueryDetailedSyncStatusForDebugging(&status);
  std::string old_cache_guid = status.sync_id;

  GetFakeServer()->TriggerError(sync_pb::SyncEnums::CLIENT_DATA_OBSOLETE);

  // Trigger sync by making one more change.
  const BookmarkNode* node2 = AddFolder(0, 0, "title2");
  SetTitle(0, node2, "new_title2");

  ASSERT_TRUE(SyncEngineStoppedChecker(GetSyncService(0)).Wait());

  // Make server return SUCCESS so that sync can initialize.
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::SUCCESS);

  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());

  // Ensure cache_guid changed.
  GetSyncService(0)->QueryDetailedSyncStatusForDebugging(&status);
  ASSERT_NE(old_cache_guid, status.sync_id);
}

IN_PROC_BROWSER_TEST_F(SyncErrorTest, DisableDatatypeWhileRunning) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  syncer::ModelTypeSet synced_datatypes =
      GetSyncService(0)->GetActiveDataTypes();
  ASSERT_TRUE(synced_datatypes.Has(syncer::TYPED_URLS));
  ASSERT_TRUE(synced_datatypes.Has(syncer::SESSIONS));
  GetProfile(0)->GetPrefs()->SetBoolean(
      prefs::kSavingBrowserHistoryDisabled, true);

  // Wait for reconfigurations.
  ASSERT_TRUE(
      TypeDisabledChecker(GetSyncService(0), syncer::TYPED_URLS).Wait());
  ASSERT_TRUE(TypeDisabledChecker(GetSyncService(0), syncer::SESSIONS).Wait());

  const BookmarkNode* node1 = AddFolder(0, 0, "title1");
  SetTitle(0, node1, "new_title1");
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  // TODO(lipalani): Verify initial sync ended for typed url is false.
}

}  // namespace
