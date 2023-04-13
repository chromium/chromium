// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/enum_set.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/fake_server.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using fake_server::FakeServer;
using sync_pb::SyncEnums;
using syncer::ModelType;
using syncer::ModelTypeSet;

namespace {

// Collects all the updated data types and used GetUpdates origins.
class GetUpdatesObserver : public FakeServer::Observer {
 public:
  explicit GetUpdatesObserver(FakeServer* fake_server)
      : fake_server_(fake_server) {
    DCHECK(fake_server);
    fake_server_->AddObserver(this);
  }

  ~GetUpdatesObserver() override { fake_server_->RemoveObserver(this); }

  using GetUpdatesOriginSet = base::EnumSet<SyncEnums::GetUpdatesOrigin,
                                            SyncEnums::GetUpdatesOrigin_MIN,
                                            SyncEnums::GetUpdatesOrigin_MAX>;

  // fake_server::FakeServer::Observer overrides.
  void OnSuccessfulGetUpdates() override {
    sync_pb::ClientToServerMessage message;
    fake_server_->GetLastGetUpdatesMessage(&message);
    DCHECK_NE(message.get_updates().get_updates_origin(),
              SyncEnums::UNKNOWN_ORIGIN);

    get_updates_origins_.Put(message.get_updates().get_updates_origin());
    for (const sync_pb::DataTypeProgressMarker& progress_marker :
         message.get_updates().from_progress_marker()) {
      ModelType type = syncer::GetModelTypeFromSpecificsFieldNumber(
          progress_marker.data_type_id());
      DCHECK_NE(type, ModelType::UNSPECIFIED);
      updated_types_.Put(type);
    }
  }

  GetUpdatesOriginSet GetAllOrigins() const { return get_updates_origins_; }

  ModelTypeSet GetUpdatedTypes() const { return updated_types_; }

 private:
  const raw_ptr<FakeServer> fake_server_;

  GetUpdatesOriginSet get_updates_origins_;
  ModelTypeSet updated_types_;
};

class SingleClientCommonSyncTest : public SyncTest {
 public:
  SingleClientCommonSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientCommonSyncTest() override = default;
  SingleClientCommonSyncTest(const SingleClientCommonSyncTest&) = delete;
  SingleClientCommonSyncTest& operator=(const SingleClientCommonSyncTest&) =
      delete;
};

// Android doesn't currently support PRE_ tests, see crbug.com/1117345.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientCommonSyncTest,
                       PRE_ShouldNotIssueGetUpdatesOnBrowserRestart) {
  ASSERT_TRUE(SetupSync());
}

IN_PROC_BROWSER_TEST_F(SingleClientCommonSyncTest,
                       ShouldNotIssueGetUpdatesOnBrowserRestart) {
  GetUpdatesObserver get_updates_observer(GetFakeServer());

  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  // Some data types may use preconditions in the model type controller to
  // postpone their startup. Since such data types were paused (even for a short
  // period), an additional GetUpdates request may be sent during initialization
  // for them.
  // TODO(crbug.com/1432855): remove once GetUpdates is not issued anymore.
  GetUpdatesObserver::GetUpdatesOriginSet get_updates_origins_to_exclude{
      SyncEnums::PROGRAMMATIC};
  ModelTypeSet types_to_exclude{ModelType::ARC_PACKAGE, ModelType::HISTORY,
                                ModelType::CONTACT_INFO, ModelType::NIGORI};

  // Verify that there were no unexpected GetUpdates requests during Sync
  // initialization.
  // TODO(crbug.com/1418329): wait for invalidations to initialize and consider
  // making a Commit request. This would help to verify that there are no
  // unnecessary GetUpdates requests after browser restart.
  EXPECT_TRUE(Difference(get_updates_observer.GetAllOrigins(),
                         get_updates_origins_to_exclude)
                  .Empty());
  EXPECT_TRUE(
      Difference(get_updates_observer.GetUpdatedTypes(), types_to_exclude)
          .Empty())
      << "Updated data types: " << get_updates_observer.GetUpdatedTypes();
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
