// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_engine_stopped_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/sync/test/integration/user_events_helper.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/sync_protocol_error.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync_user_events/user_event_service.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/google_service_auth_error.h"

using bookmarks::BookmarkNode;
using bookmarks_helper::AddFolder;
using bookmarks_helper::SetTitle;
using syncer::SyncServiceImpl;
using testing::IsEmpty;
using user_events_helper::CreateTestEvent;

namespace {

constexpr int64_t kUserEventTimeUsec = 123456;

syncer::DataTypeSet GetThrottledDataTypes(
    syncer::SyncServiceImpl* sync_service) {
  base::RunLoop loop;
  syncer::DataTypeSet throttled_types;
  sync_service->GetThrottledDataTypesForTest(
      base::BindLambdaForTesting([&](syncer::DataTypeSet result) {
        throttled_types = result;
        loop.Quit();
      }));
  loop.Run();
  return throttled_types;
}

size_t GetTypeNonTombstoneEntitiesCount(
    syncer::DataTypeControllerDelegate* data_type_controller_delegate) {
  base::RunLoop loop;
  size_t result = 0;
  data_type_controller_delegate->GetTypeEntitiesCountForDebugging(
      base::BindLambdaForTesting(
          [&result, &loop](const syncer::TypeEntitiesCount& count) {
            result = count.non_tombstone_entities;
            loop.Quit();
          }));
  loop.Run();
  return result;
}

class TypeDisabledChecker : public SingleClientStatusChangeChecker {
 public:
  explicit TypeDisabledChecker(SyncServiceImpl* service, syncer::DataType type)
      : SingleClientStatusChangeChecker(service), type_(type) {}

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for type " << syncer::DataTypeToDebugString(type_)
        << " to become disabled";
    return !service()->GetActiveDataTypes().Has(type_);
  }

 private:
  syncer::DataType type_;
};

// Wait for a commit message containing the expected user event (even if the
// commit request fails).
class UserEventCommitChecker : public SingleClientStatusChangeChecker {
 public:
  UserEventCommitChecker(SyncServiceImpl* service,
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
  const raw_ptr<fake_server::FakeServer> fake_server_ = nullptr;
  const int64_t expected_event_time_usec_;
};

class SyncErrorTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SyncErrorTest() : SyncTest(SINGLE_CLIENT) {
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      scoped_feature_list_.InitAndEnableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    }
  }

  SyncErrorTest(const SyncErrorTest&) = delete;
  SyncErrorTest& operator=(const SyncErrorTest&) = delete;

  ~SyncErrorTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  const bookmarks::BookmarkNode* GetParent() {
    bookmarks::BookmarkModel* model = bookmarks_helper::GetBookmarkModel(0);
    switch (GetSetupSyncMode()) {
      case SetupSyncMode::kSyncTransportOnly:
        return model->account_bookmark_bar_node();
      case SetupSyncMode::kSyncTheFeature:
        return model->bookmark_bar_node();
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SyncErrorTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

// Helper class that waits until the sync engine has hit an actionable error.
class ActionableProtocolErrorChecker : public SingleClientStatusChangeChecker {
 public:
  explicit ActionableProtocolErrorChecker(SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {}

  ActionableProtocolErrorChecker(const ActionableProtocolErrorChecker&) =
      delete;
  ActionableProtocolErrorChecker& operator=(
      const ActionableProtocolErrorChecker&) = delete;

  ~ActionableProtocolErrorChecker() override = default;

  // Checks if an actionable error has been hit. Called repeatedly each time PSS
  // notifies observers of a state change.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting until sync hits an actionable error";
    syncer::SyncStatus status;
    service()->QueryDetailedSyncStatusForDebugging(&status);
    return (status.sync_protocol_error.action != syncer::UNKNOWN_ACTION &&
            service()->HasUnrecoverableError());
  }
};

IN_PROC_BROWSER_TEST_P(SyncErrorTest, BirthdayErrorTest) {
  ASSERT_TRUE(SetupSync());

  // Clearing the server data resets the server birthday and triggers a NIGORI
  // invalidation. This triggers a sync cycle and a GetUpdates request that runs
  // into NOT_MY_BIRTHDAY.
  GetFakeServer()->ClearServerData();

  ASSERT_TRUE(syncer::SyncEngineStoppedChecker(GetSyncService(0)).Wait());

#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsSyncFeatureDisabledViaDashboard());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

IN_PROC_BROWSER_TEST_P(SyncErrorTest, UpgradeClientErrorDuringIncrementalSync) {
  ASSERT_TRUE(SetupSync());

  const BookmarkNode* node1 = AddFolder(0, GetParent(), 0, u"title1");
  SetTitle(0, node1, u"new_title1");
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  std::string description = "Not My Fault";
  std::string url = "www.google.com";
  GetFakeServer()->TriggerActionableProtocolError(
      sync_pb::SyncEnums::THROTTLED, description, url,
      sync_pb::SyncEnums::UPGRADE_CLIENT);

  // Now make one more change so we will do another sync.
  const BookmarkNode* node2 = AddFolder(0, GetParent(), 0, u"title2");
  SetTitle(0, node2, u"new_title2");

  // Wait until an actionable error is encountered.
  EXPECT_TRUE(ActionableProtocolErrorChecker(GetSyncService(0)).Wait());

  // UPGRADE_CLIENT gets mapped to an unrecoverable error, so Sync will *not*
  // start up again in transport-only mode (which would clear the cached error).
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
            GetSyncService(0)->GetTransportState());

  syncer::SyncStatus status;
  GetSyncService(0)->QueryDetailedSyncStatusForDebugging(&status);
  EXPECT_EQ(status.sync_protocol_error.error_type, syncer::THROTTLED);
  EXPECT_EQ(status.sync_protocol_error.action, syncer::UPGRADE_CLIENT);
  EXPECT_EQ(status.sync_protocol_error.error_description, description);
}

IN_PROC_BROWSER_TEST_P(SyncErrorTest, UpgradeClientErrorDuringInitialSync) {
  std::string description = "Not My Fault";
  std::string url = "www.google.com";
  GetFakeServer()->TriggerActionableProtocolError(
      sync_pb::SyncEnums::THROTTLED, description, url,
      sync_pb::SyncEnums::UPGRADE_CLIENT);

  ASSERT_TRUE(SetupClients());

  // Signing in should start sync-the-transport, which should fail with an
  // error.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());

  // Wait until an actionable error is encountered.
  EXPECT_TRUE(ActionableProtocolErrorChecker(GetSyncService(0)).Wait());

  // UPGRADE_CLIENT gets mapped to an unrecoverable error, so Sync will *not*
  // start up again in transport-only mode (which would clear the cached error).
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
            GetSyncService(0)->GetTransportState());

  syncer::SyncStatus status;
  GetSyncService(0)->QueryDetailedSyncStatusForDebugging(&status);
  EXPECT_EQ(status.sync_protocol_error.error_type, syncer::THROTTLED);
  EXPECT_EQ(status.sync_protocol_error.action, syncer::UPGRADE_CLIENT);
  EXPECT_EQ(status.sync_protocol_error.error_description, description);
}

// This test verifies that sync keeps retrying if it encounters error during
// setup.
// crbug.com/689662
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ErrorWhileSettingUp DISABLED_ErrorWhileSettingUp
#else
#define MAYBE_ErrorWhileSettingUp ErrorWhileSettingUp
#endif
IN_PROC_BROWSER_TEST_P(SyncErrorTest, MAYBE_ErrorWhileSettingUp) {
  ASSERT_TRUE(SetupClients());

#if !BUILDFLAG(IS_CHROMEOS)
  // On non auto start enabled environments if the setup sync fails then
  // the setup would fail. So setup sync normally.
  // In contrast on auto start enabled platforms like chrome os we should be
  // able to set up even if the first sync while setting up fails.
  ASSERT_TRUE(SetupSync()) << "Setup sync failed";
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kAutofill));
#endif

  GetFakeServer()->TriggerError(sync_pb::SyncEnums::TRANSIENT_ERROR);
  EXPECT_TRUE(GetFakeServer()->EnableAlternatingTriggeredErrors());

#if BUILDFLAG(IS_CHROMEOS)
  // Now setup sync and it should succeed.
  ASSERT_TRUE(SetupSync());
#else
  // Now enable a datatype, whose first 2 syncs would fail, but we should
  // recover and setup succesfully on the third attempt.
  ASSERT_TRUE(GetClient(0)->EnableSelectableType(
      syncer::UserSelectableType::kAutofill));
#endif
}

// Tests that on receiving CLIENT_DATA_OBSOLETE sync engine gets restarted and
// initialized with different cache_guid.
IN_PROC_BROWSER_TEST_P(SyncErrorTest, ClientDataObsoleteTest) {
  ASSERT_TRUE(SetupSync());

  const BookmarkNode* node1 = AddFolder(0, GetParent(), 0, u"title1");
  SetTitle(0, node1, u"new_title1");
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  std::string description = "Not My Fault";
  std::string url = "www.google.com";

  // Remember cache_guid before actionable error.
  syncer::SyncStatus status;
  GetSyncService(0)->QueryDetailedSyncStatusForDebugging(&status);
  std::string old_cache_guid = status.cache_guid;

  GetFakeServer()->TriggerError(sync_pb::SyncEnums::CLIENT_DATA_OBSOLETE);

  // Trigger sync by making one more change.
  const BookmarkNode* node2 = AddFolder(0, GetParent(), 0, u"title2");
  SetTitle(0, node2, u"new_title2");

  ASSERT_TRUE(syncer::SyncEngineStoppedChecker(GetSyncService(0)).Wait());

  // Make server return SUCCESS so that sync can initialize.
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::SUCCESS);

  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());

  // Ensure cache_guid changed.
  GetSyncService(0)->QueryDetailedSyncStatusForDebugging(&status);
  ASSERT_NE(old_cache_guid, status.cache_guid);
}

IN_PROC_BROWSER_TEST_P(SyncErrorTest, EncryptionObsoleteErrorTest) {
  ASSERT_TRUE(SetupSync());

  const BookmarkNode* node1 = AddFolder(0, GetParent(), 0, u"title1");
  SetTitle(0, node1, u"new_title1");
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  GetFakeServer()->TriggerActionableProtocolError(
      sync_pb::SyncEnums::ENCRYPTION_OBSOLETE, "Not My Fault", "www.google.com",
      sync_pb::SyncEnums::UNKNOWN_ACTION);

  // Now make one more change so we will do another sync.
  const BookmarkNode* node2 = AddFolder(0, GetParent(), 0, u"title2");
  SetTitle(0, node2, u"new_title2");

  syncer::SyncEngineStoppedChecker sync_stopped_waiter(GetSyncService(0));
  ASSERT_TRUE(sync_stopped_waiter.Wait());

  // On receiving the error, the SyncService will immediately start up again
  // in transport mode, which resets the status. So check the status that the
  // checker recorded at the time Sync was off.
  syncer::SyncStatus status = sync_stopped_waiter.status_on_engine_stopped();
  EXPECT_EQ(status.sync_protocol_error.error_type, syncer::ENCRYPTION_OBSOLETE);
  EXPECT_EQ(status.sync_protocol_error.action, syncer::DISABLE_SYNC_ON_CLIENT);
}

IN_PROC_BROWSER_TEST_P(SyncErrorTest, DisableDatatypeWhileRunning) {
  ASSERT_TRUE(SetupSync());
  syncer::DataTypeSet synced_datatypes =
      GetSyncService(0)->GetActiveDataTypes();
  ASSERT_TRUE(synced_datatypes.Has(syncer::HISTORY));
  ASSERT_TRUE(synced_datatypes.Has(syncer::SESSIONS));
  GetProfile(0)->GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled,
                                        true);

  // Wait for reconfigurations.
  ASSERT_TRUE(TypeDisabledChecker(GetSyncService(0), syncer::HISTORY).Wait());
  ASSERT_TRUE(TypeDisabledChecker(GetSyncService(0), syncer::SESSIONS).Wait());

  const BookmarkNode* node1 = AddFolder(0, GetParent(), 0, u"title1");
  SetTitle(0, node1, u"new_title1");
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
}

// Tests that the unsynced entity will be eventually committed even after failed
// commit request.
IN_PROC_BROWSER_TEST_P(SyncErrorTest,
                       PRE_ShouldResendUncommittedEntitiesAfterBrowserRestart) {
  ASSERT_TRUE(SetupSync());

  GetFakeServer()->TriggerCommitError(sync_pb::SyncEnums::TRANSIENT_ERROR);
  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));
  const sync_pb::UserEventSpecifics specifics =
      CreateTestEvent(base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(kUserEventTimeUsec)));
  event_service->RecordUserEvent(
      std::make_unique<sync_pb::UserEventSpecifics>(specifics));

  // Wait for a commit message containing the user event. However the commit
  // request will fail.
  ASSERT_TRUE(UserEventCommitChecker(GetSyncService(0), GetFakeServer(),
                                     kUserEventTimeUsec)
                  .Wait());

  // Check that the server doesn't have this event yet.
  for (const sync_pb::SyncEntity& entity :
       GetFakeServer()->GetSyncEntitiesByDataType(syncer::USER_EVENTS)) {
    ASSERT_NE(kUserEventTimeUsec,
              entity.specifics().user_event().event_time_usec());
  }
}

IN_PROC_BROWSER_TEST_P(SyncErrorTest,
                       ShouldResendUncommittedEntitiesAfterBrowserRestart) {
  // Make sure the PRE_ test didn't successfully commit the event.
  ASSERT_THAT(GetFakeServer()->GetSyncEntitiesByDataType(syncer::USER_EVENTS),
              IsEmpty());

  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  const sync_pb::UserEventSpecifics expected_specifics =
      CreateTestEvent(base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(kUserEventTimeUsec)));
  EXPECT_TRUE(UserEventEqualityChecker(GetSyncService(0), GetFakeServer(),
                                       {{expected_specifics}})
                  .Wait())
      << "Non-tombstone entities: "
      << GetTypeNonTombstoneEntitiesCount(
             browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0))
                 ->GetControllerDelegate()
                 .get());
}

// Tests that throttling one datatype does not influence other datatypes.
IN_PROC_BROWSER_TEST_P(SyncErrorTest, ShouldThrottleOneDatatypeButNotOthers) {
  const std::u16string kBookmarkFolderTitle = u"title1";

  ASSERT_TRUE(SetupSync());
  // Set the preference to false initially which should get synced.
  GetProfile(0)->GetPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::DataType::PREFERENCES, prefs::kHomePageIsNewTabPage,
                  preferences_helper::ConvertPrefValueToValueInSpecifics(
                      base::Value(false)))
                  .Wait());

  // Start throttling PREFERENCES so further commits will be rejected by the
  // server.
  GetFakeServer()->SetThrottledTypes({syncer::PREFERENCES});

  // Make local changes for PREFERENCES and BOOKMARKS, but the first is
  // throttled.
  GetProfile(0)->GetPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage, true);
  AddFolder(0, GetParent(), 0, kBookmarkFolderTitle);

  // The bookmark should get committed successfully.
  EXPECT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  {{kBookmarkFolderTitle, GURL()}},
                  /*cryptographer=*/nullptr)
                  .Wait());

  // The preference should remain unsynced (still set to the previous value).
  EXPECT_EQ(preferences_helper::GetPreferenceInFakeServer(
                syncer::DataType::PREFERENCES, prefs::kHomePageIsNewTabPage,
                GetFakeServer())
                ->value(),
            "false");

  // PREFERENCES should now be throttled.
  EXPECT_EQ(GetThrottledDataTypes(GetSyncService(0)),
            syncer::DataTypeSet({syncer::PREFERENCES}));

  // Unthrottle PREFERENCES to verify that sync can resume.
  GetFakeServer()->SetThrottledTypes(syncer::DataTypeSet());

  // Eventually (depending on throttling delay, which is short in tests) the
  // preference should be committed.
  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(syncer::DataType::PREFERENCES,
                                                prefs::kHomePageIsNewTabPage,
                                                "true")
                  .Wait());
  EXPECT_EQ(GetThrottledDataTypes(GetSyncService(0)), syncer::DataTypeSet());
}

}  // namespace
