// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_disabled_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/sync/test/integration/user_events_helper.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/protocol/sync_protocol_error.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync_user_events/user_event_service.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/google_service_auth_error.h"

using bookmarks::BookmarkNode;
using bookmarks_helper::AddFolder;
using bookmarks_helper::SetTitle;
using syncer::ProfileSyncService;
using user_events_helper::CreateTestEvent;

namespace {

constexpr int64_t kUserEventTimeUsec = 123456;

syncer::ModelTypeSet GetThrottledDataTypes(
    syncer::ProfileSyncService* sync_service) {
  base::RunLoop loop;
  syncer::ModelTypeSet throttled_types;
  sync_service->GetThrottledDataTypesForTest(
      base::BindLambdaForTesting([&](syncer::ModelTypeSet result) {
        throttled_types = result;
        loop.Quit();
      }));
  loop.Run();
  return throttled_types;
}

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

// Wait for a commit message containing the expected user event (even if the
// commit request fails).
class UserEventCommitChecker : public SingleClientStatusChangeChecker {
 public:
  UserEventCommitChecker(ProfileSyncService* service,
                         fake_server::FakeServer* fake_server,
                         int64_t expected_event_time_usec)
      : SingleClientStatusChangeChecker(service),
        fake_server_(fake_server),
        expected_event_time_usec_(expected_event_time_usec) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for user event to be committed";

    sync_pb::ClientToServerMessage message;
    fake_server_->GetLastCommitMessage(&message);
    for (const sync_pb::SyncEntity& entity : message.commit().entries()) {
      if (entity.specifics().user_event().event_time_usec() ==
          expected_event_time_usec_) {
        return true;
      }
    }
    return false;
  }

 private:
  fake_server::FakeServer* const fake_server_ = nullptr;
  const int64_t expected_event_time_usec_;
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

  // UPGRADE_CLIENT gets mapped to an unrecoverable error, so Sync will *not*
  // start up again in transport-only mode (which would clear the cached error).
  syncer::SyncStatus status;
  GetSyncService(0)->QueryDetailedSyncStatusForDebugging(&status);
  ASSERT_EQ(status.sync_protocol_error.error_type, syncer::TRANSIENT_ERROR);
  ASSERT_EQ(status.sync_protocol_error.action, syncer::UPGRADE_CLIENT);
  ASSERT_EQ(status.sync_protocol_error.error_description, description);
}

// This test verifies that sync keeps retrying if it encounters error during
// setup.
// crbug.com/689662
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_ErrorWhileSettingUp DISABLED_ErrorWhileSettingUp
#else
#define MAYBE_ErrorWhileSettingUp ErrorWhileSettingUp
#endif
IN_PROC_BROWSER_TEST_F(SyncErrorTest, MAYBE_ErrorWhileSettingUp) {
  ASSERT_TRUE(SetupClients());

#if !BUILDFLAG(IS_CHROMEOS_ASH)
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

  SyncDisabledChecker sync_disabled(GetSyncService(0));
  sync_disabled.Wait();

  // On receiving the error, the SyncService will immediately start up again
  // in transport mode, which resets the status. So check the status that the
  // checker recorded at the time Sync was off.
  syncer::SyncStatus status = sync_disabled.status_on_sync_disabled();
  EXPECT_EQ(status.sync_protocol_error.error_type, syncer::NOT_MY_BIRTHDAY);
  EXPECT_EQ(status.sync_protocol_error.action, syncer::DISABLE_SYNC_ON_CLIENT);
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

IN_PROC_BROWSER_TEST_F(SyncErrorTest, EncryptionObsoleteErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* node1 = AddFolder(0, 0, "title1");
  SetTitle(0, node1, "new_title1");
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  GetFakeServer()->TriggerActionableError(
      sync_pb::SyncEnums::ENCRYPTION_OBSOLETE, "Not My Fault", "www.google.com",
      sync_pb::SyncEnums::UNKNOWN_ACTION);

  // Now make one more change so we will do another sync.
  const BookmarkNode* node2 = AddFolder(0, 0, "title2");
  SetTitle(0, node2, "new_title2");

  SyncDisabledChecker sync_disabled(GetSyncService(0));
  sync_disabled.Wait();

  // On receiving the error, the SyncService will immediately start up again
  // in transport mode, which resets the status. So check the status that the
  // checker recorded at the time Sync was off.
  syncer::SyncStatus status = sync_disabled.status_on_sync_disabled();
  EXPECT_EQ(status.sync_protocol_error.error_type, syncer::ENCRYPTION_OBSOLETE);
  EXPECT_EQ(status.sync_protocol_error.action, syncer::DISABLE_SYNC_ON_CLIENT);
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

// Tests that the unsynced entity will be eventually committed even after failed
// commit request.
IN_PROC_BROWSER_TEST_F(SyncErrorTest,
                       PRE_ShouldResendUncommittedEntitiesAfterBrowserRestart) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  GetFakeServer()->SetHttpError(net::HTTP_INTERNAL_SERVER_ERROR);
  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));
  const sync_pb::UserEventSpecifics specifics =
      CreateTestEvent(base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMicroseconds(kUserEventTimeUsec)));
  event_service->RecordUserEvent(specifics);

  // Wait for a commit message containing the user event. However the commit
  // request will fail.
  ASSERT_TRUE(UserEventCommitChecker(GetSyncService(0), GetFakeServer(),
                                     kUserEventTimeUsec)
                  .Wait());

  // Check that the server doesn't have this event yet.
  for (const sync_pb::SyncEntity& entity :
       GetFakeServer()->GetSyncEntitiesByModelType(syncer::USER_EVENTS)) {
    ASSERT_NE(kUserEventTimeUsec,
              entity.specifics().user_event().event_time_usec());
  }
}

IN_PROC_BROWSER_TEST_F(SyncErrorTest,
                       ShouldResendUncommittedEntitiesAfterBrowserRestart) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());

  const sync_pb::UserEventSpecifics expected_specifics =
      CreateTestEvent(base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMicroseconds(kUserEventTimeUsec)));
  EXPECT_TRUE(UserEventEqualityChecker(GetSyncService(0), GetFakeServer(),
                                       {{expected_specifics}})
                  .Wait());
}

// Tests that throttling one datatype does not influence other datatypes.
IN_PROC_BROWSER_TEST_F(SyncErrorTest, ShouldThrottleOneDatatypeButNotOthers) {
  const std::string kBookmarkFolderTitle = "title1";

  ASSERT_TRUE(SetupClients());

  // Set the preference to false initially which should get synced.
  GetProfile(0)->GetPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(preferences_helper::GetPreferenceInFakeServer(
                  prefs::kHomePageIsNewTabPage, GetFakeServer())
                  .has_value());
  ASSERT_EQ(preferences_helper::GetPreferenceInFakeServer(
                prefs::kHomePageIsNewTabPage, GetFakeServer())
                ->value(),
            "false");

  // Start throttling PREFERENCES so further commits will be rejected by the
  // server.
  GetFakeServer()->SetThrottledTypes({syncer::PREFERENCES});

  // Make local changes for PREFERENCES and BOOKMARKS, but the first is
  // throttled.
  GetProfile(0)->GetPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage, true);
  AddFolder(0, 0, kBookmarkFolderTitle);

  // The bookmark should get committed successfully.
  EXPECT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  GetSyncService(0), GetFakeServer(),
                  {{kBookmarkFolderTitle, GURL()}},
                  /*cryptographer=*/nullptr)
                  .Wait());

  // The preference should remain unsynced (still set to the previous value).
  EXPECT_EQ(preferences_helper::GetPreferenceInFakeServer(
                prefs::kHomePageIsNewTabPage, GetFakeServer())
                ->value(),
            "false");

  // PREFERENCES should now be throttled.
  EXPECT_EQ(GetThrottledDataTypes(GetSyncService(0)),
            syncer::ModelTypeSet{syncer::PREFERENCES});

  // Unthrottle PREFERENCES to verify that sync can resume.
  GetFakeServer()->SetThrottledTypes(syncer::ModelTypeSet());

  // Eventually (depending on throttling delay, which is short in tests) the
  // preference should be committed.
  EXPECT_TRUE(
      FakeServerPrefMatchesValueChecker(prefs::kHomePageIsNewTabPage, "true")
          .Wait());
  EXPECT_EQ(GetThrottledDataTypes(GetSyncService(0)), syncer::ModelTypeSet());
}

}  // namespace
