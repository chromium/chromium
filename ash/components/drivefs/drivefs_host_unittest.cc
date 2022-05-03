// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/drivefs/drivefs_host.h"

#include <set>
#include <type_traits>
#include <utility>

#include "ash/components/disks/disk_mount_manager.h"
#include "ash/components/disks/mock_disk_mount_manager.h"
#include "ash/components/drivefs/drivefs_host_observer.h"
#include "ash/components/drivefs/fake_drivefs.h"
#include "ash/components/drivefs/mojom/drivefs.mojom-test-utils.h"
#include "ash/components/drivefs/mojom/drivefs.mojom.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/timer/mock_timer.h"
#include "chromeos/components/mojo_bootstrap/pending_connection_manager.h"
#include "components/drive/drive_notification_manager.h"
#include "components/drive/drive_notification_observer.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

namespace drivefs {
namespace {

using base::test::RunOnceClosure;
using testing::_;
using MountFailure = DriveFsHost::MountObserver::MountFailure;

constexpr base::TimeDelta kTokenLifetime = base::Hours(1);

class MockDriveFs : public mojom::DriveFsInterceptorForTesting,
                    public mojom::SearchQuery {
 public:
  MockDriveFs() = default;

  DriveFs* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  void FetchChangeLog(std::vector<mojom::FetchChangeLogOptionsPtr> options) {
    std::vector<std::pair<int64_t, std::string>> unwrapped_options;
    for (auto& entry : options) {
      unwrapped_options.push_back(
          std::make_pair(entry->change_id, entry->team_drive_id));
    }
    FetchChangeLogImpl(unwrapped_options);
  }

  MOCK_METHOD1(
      FetchChangeLogImpl,
      void(const std::vector<std::pair<int64_t, std::string>>& options));
  MOCK_METHOD0(FetchAllChangeLogs, void());

  MOCK_CONST_METHOD1(OnStartSearchQuery, void(const mojom::QueryParameters&));
  void StartSearchQuery(mojo::PendingReceiver<mojom::SearchQuery> receiver,
                        mojom::QueryParametersPtr query_params) override {
    search_receiver_.reset();
    OnStartSearchQuery(*query_params);
    search_receiver_.Bind(std::move(receiver));
  }

  MOCK_METHOD1(OnGetNextPage,
               drive::FileError(
                   absl::optional<std::vector<mojom::QueryItemPtr>>* items));

  void GetNextPage(GetNextPageCallback callback) override {
    absl::optional<std::vector<mojom::QueryItemPtr>> items;
    auto error = OnGetNextPage(&items);
    std::move(callback).Run(error, std::move(items));
  }

 private:
  mojo::Receiver<mojom::SearchQuery> search_receiver_{this};
};

class TestingDriveFsHostDelegate : public DriveFsHost::Delegate,
                                   public DriveFsHost::MountObserver {
 public:
  TestingDriveFsHostDelegate(signin::IdentityManager* identity_manager,
                             const AccountId& account_id)
      : identity_manager_(identity_manager),
        account_id_(account_id),
        drive_notification_manager_(&invalidation_service_) {}

  TestingDriveFsHostDelegate(const TestingDriveFsHostDelegate&) = delete;
  TestingDriveFsHostDelegate& operator=(const TestingDriveFsHostDelegate&) =
      delete;

  ~TestingDriveFsHostDelegate() override {
    drive_notification_manager_.Shutdown();
  }

  void set_pending_bootstrap(
      mojo::PendingRemote<mojom::DriveFsBootstrap> pending_bootstrap) {
    pending_bootstrap_ = std::move(pending_bootstrap);
  }

  void set_verbose_logging_enabled(bool enabled) {
    verbose_logging_enabled_ = enabled;
  }

  drivefs::mojom::ExtensionConnectionParams& get_last_extension_params() {
    return *extension_params_;
  }

  // DriveFsHost::MountObserver:
  MOCK_METHOD1(OnMounted, void(const base::FilePath&));
  MOCK_METHOD2(OnMountFailed,
               void(MountFailure, absl::optional<base::TimeDelta>));
  MOCK_METHOD1(OnUnmounted, void(absl::optional<base::TimeDelta>));

  drive::DriveNotificationManager& GetDriveNotificationManager() override {
    return drive_notification_manager_;
  }

 private:
  // DriveFsHost::Delegate:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  signin::IdentityManager* GetIdentityManager() override {
    return identity_manager_;
  }
  const AccountId& GetAccountId() override { return account_id_; }
  std::string GetObfuscatedAccountId() override {
    return "salt-" + account_id_.GetAccountIdKey();
  }
  bool IsMetricsCollectionEnabled() override { return false; }

  std::unique_ptr<DriveFsBootstrapListener> CreateMojoListener() override {
    DCHECK(pending_bootstrap_);
    return std::make_unique<FakeDriveFsBootstrapListener>(
        std::move(pending_bootstrap_));
  }

  std::string GetLostAndFoundDirectoryName() override {
    return "recovered files";
  }

  base::FilePath GetMyFilesPath() override {
    return base::FilePath("/MyFiles");
  }

  bool IsVerboseLoggingEnabled() override { return verbose_logging_enabled_; }

  drivefs::mojom::DriveFsDelegate::ExtensionConnectionStatus ConnectToExtension(
      drivefs::mojom::ExtensionConnectionParamsPtr params,
      mojo::PendingReceiver<drivefs::mojom::NativeMessagingPort> port,
      mojo::PendingRemote<drivefs::mojom::NativeMessagingHost> host) override {
    extension_params_ = std::move(params);
    return drivefs::mojom::DriveFsDelegate::ExtensionConnectionStatus::
        kExtensionNotFound;
  }

  signin::IdentityManager* const identity_manager_;
  const AccountId account_id_;
  mojo::PendingRemote<mojom::DriveFsBootstrap> pending_bootstrap_;
  bool verbose_logging_enabled_ = false;
  invalidation::FakeInvalidationService invalidation_service_;
  drive::DriveNotificationManager drive_notification_manager_;
  drivefs::mojom::ExtensionConnectionParamsPtr extension_params_;
};

class MockDriveFsHostObserver : public DriveFsHostObserver {
 public:
  MOCK_METHOD0(OnUnmounted, void());
  MOCK_METHOD1(OnSyncingStatusUpdate, void(const mojom::SyncingStatus& status));
  MOCK_METHOD1(OnFilesChanged,
               void(const std::vector<mojom::FileChange>& changes));
  MOCK_METHOD1(OnError, void(const mojom::DriveError& error));
};

class DriveFsHostTest : public ::testing::Test, public mojom::DriveFsBootstrap {
 public:
  DriveFsHostTest()
      : network_connection_tracker_(
            network::TestNetworkConnectionTracker::CreateInstance()) {
    clock_.SetNow(base::Time::Now());
  }

  DriveFsHostTest(const DriveFsHostTest&) = delete;
  DriveFsHostTest& operator=(const DriveFsHostTest&) = delete;

 protected:
  void SetUp() override {
    testing::Test::SetUp();
    profile_path_ = base::FilePath(FILE_PATH_LITERAL("/path/to/profile"));
    account_id_ = AccountId::FromUserEmailGaiaId("test@example.com", "ID");

    disk_manager_ = std::make_unique<ash::disks::MockDiskMountManager>();
    identity_test_env_.MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
    host_delegate_ = std::make_unique<TestingDriveFsHostDelegate>(
        identity_test_env_.identity_manager(), account_id_);
    auto timer = std::make_unique<base::MockOneShotTimer>();
    timer_ = timer.get();
    host_ = std::make_unique<DriveFsHost>(
        profile_path_, host_delegate_.get(), host_delegate_.get(),
        network_connection_tracker_.get(), &clock_, disk_manager_.get(),
        std::move(timer));
  }

  void TearDown() override {
    host_.reset();
    disk_manager_.reset();
  }

  std::string StartMount() {
    std::string source;
    EXPECT_CALL(
        *disk_manager_,
        MountPath(
            testing::StartsWith("drivefs://"), "", "drivefs-salt-g-ID",
            testing::AllOf(testing::Contains(
                               "datadir=/path/to/profile/GCache/v2/salt-g-ID"),
                           testing::Contains("myfiles=/MyFiles")),
            _, chromeos::MOUNT_ACCESS_MODE_READ_WRITE, _))
        .WillOnce(testing::DoAll(testing::SaveArg<0>(&source),
                                 MoveArg<6>(&mount_callback_)));

    host_delegate_->set_pending_bootstrap(
        bootstrap_receiver_.BindNewPipeAndPassRemote());
    pending_delegate_receiver_ = delegate_.BindNewPipeAndPassReceiver();

    EXPECT_TRUE(host_->Mount());
    testing::Mock::VerifyAndClear(&disk_manager_);

    return source.substr(strlen("drivefs://"));
  }

  void CallMountCallbackSuccess(const std::string& token) {
    std::move(mount_callback_)
        .Run(chromeos::MOUNT_ERROR_NONE, {base::StrCat({"drivefs://", token}),
                                          "/media/drivefsroot/salt-g-ID",
                                          chromeos::MOUNT_TYPE_NETWORK_STORAGE,
                                          {}});
  }

  void SendOnMounted() { delegate_->OnMounted(); }

  void SendOnUnmounted(absl::optional<base::TimeDelta> delay) {
    delegate_->OnUnmounted(std::move(delay));
  }

  void SendMountFailed(absl::optional<base::TimeDelta> delay) {
    delegate_->OnMountFailed(std::move(delay));
  }

  void EstablishConnection() {
    token_ = StartMount();
    CallMountCallbackSuccess(token_);

    ASSERT_TRUE(mojo_bootstrap::PendingConnectionManager::Get().OpenIpcChannel(
        token_, {}));
    {
      base::RunLoop run_loop;
      bootstrap_receiver_.set_disconnect_handler(run_loop.QuitClosure());
      run_loop.Run();
    }
  }

  void DoMount() {
    EstablishConnection();
    base::RunLoop run_loop;
    base::OnceClosure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(*host_delegate_,
                OnMounted(base::FilePath("/media/drivefsroot/salt-g-ID")))
        .WillOnce(RunOnceClosure(std::move(quit_closure)));
    // Eventually we must attempt unmount.
    EXPECT_CALL(*disk_manager_, UnmountPath("/media/drivefsroot/salt-g-ID", _));
    SendOnMounted();
    run_loop.Run();
    ASSERT_TRUE(host_->IsMounted());
  }

  void DoUnmount() {
    EXPECT_CALL(*host_delegate_, OnUnmounted(_)).Times(0);
    host_->Unmount();
    receiver_.reset();
    bootstrap_receiver_.reset();
    delegate_.reset();
    base::RunLoop().RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(disk_manager_.get());
    testing::Mock::VerifyAndClearExpectations(host_delegate_.get());
  }

  void Init(mojom::DriveFsConfigurationPtr config,
            mojo::PendingReceiver<mojom::DriveFs> drive_fs_receiver,
            mojo::PendingRemote<mojom::DriveFsDelegate> delegate) override {
    EXPECT_EQ("test@example.com", config->user_email);
    EXPECT_EQ("recovered files",
              config->lost_and_found_directory_name.value_or("<None>"));
    verbose_logging_enabled_ = config->enable_verbose_logging;
    init_access_token_ = std::move(config->access_token);
    receiver_.Bind(std::move(drive_fs_receiver));
    mojo::FusePipes(std::move(pending_delegate_receiver_), std::move(delegate));
  }

  base::FilePath profile_path_;
  base::test::TaskEnvironment task_environment_;
  AccountId account_id_;
  std::unique_ptr<ash::disks::MockDiskMountManager> disk_manager_;
  ash::disks::DiskMountManager::MountPathCallback mount_callback_;
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;
  base::SimpleTestClock clock_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestingDriveFsHostDelegate> host_delegate_;
  std::unique_ptr<DriveFsHost> host_;
  base::MockOneShotTimer* timer_;
  absl::optional<bool> verbose_logging_enabled_;

  mojo::Receiver<mojom::DriveFsBootstrap> bootstrap_receiver_{this};
  MockDriveFs mock_drivefs_;
  mojo::Receiver<mojom::DriveFs> receiver_{&mock_drivefs_};
  mojo::Remote<mojom::DriveFsDelegate> delegate_;
  mojo::PendingReceiver<mojom::DriveFsDelegate> pending_delegate_receiver_;
  std::string token_;
  absl::optional<std::string> init_access_token_;
};

TEST_F(DriveFsHostTest, Basic) {
  MockDriveFsHostObserver observer;
  base::ScopedObservation<DriveFsHost, DriveFsHostObserver> observation_scoper(
      &observer);
  observation_scoper.Observe(host_.get());

  EXPECT_FALSE(host_->IsMounted());

  EXPECT_EQ(base::FilePath("/path/to/profile/GCache/v2/salt-g-ID"),
            host_->GetDataPath());

  ASSERT_NO_FATAL_FAILURE(DoMount());
  EXPECT_FALSE(init_access_token_);
  ASSERT_TRUE(verbose_logging_enabled_);
  EXPECT_FALSE(verbose_logging_enabled_.value());

  EXPECT_EQ(base::FilePath("/media/drivefsroot/salt-g-ID"),
            host_->GetMountPath());

  EXPECT_CALL(observer, OnUnmounted());
  EXPECT_CALL(*host_delegate_, OnUnmounted(_)).Times(0);
  base::RunLoop run_loop;
  delegate_.set_disconnect_handler(run_loop.QuitClosure());
  host_->Unmount();
  run_loop.Run();
}

TEST_F(DriveFsHostTest, EnableVerboseLogging) {
  ASSERT_FALSE(host_->IsMounted());

  host_delegate_->set_verbose_logging_enabled(true);
  ASSERT_NO_FATAL_FAILURE(DoMount());
  ASSERT_TRUE(verbose_logging_enabled_);
  EXPECT_TRUE(verbose_logging_enabled_.value());
}

TEST_F(DriveFsHostTest, GetMountPathWhileUnmounted) {
  EXPECT_EQ(base::FilePath("/media/fuse/drivefs-salt-g-ID"),
            host_->GetMountPath());
}

TEST_F(DriveFsHostTest, OnMountFailedFromMojo) {
  ASSERT_FALSE(host_->IsMounted());

  ASSERT_NO_FATAL_FAILURE(EstablishConnection());
  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*host_delegate_, OnMountFailed(MountFailure::kUnknown, _))
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  SendMountFailed({});
  run_loop.Run();
  ASSERT_FALSE(host_->IsMounted());
}

TEST_F(DriveFsHostTest, OnMountFailedFromDbus) {
  ASSERT_FALSE(host_->IsMounted());
  EXPECT_CALL(*disk_manager_, UnmountPath(_, _)).Times(0);

  auto token = StartMount();

  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*host_delegate_, OnMountFailed(MountFailure::kInvocation, _))
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  std::move(mount_callback_)
      .Run(chromeos::MOUNT_ERROR_INVALID_MOUNT_OPTIONS,
           {base::StrCat({"drivefs://", token}),
            "/media/drivefsroot/salt-g-ID",
            chromeos::MOUNT_TYPE_NETWORK_STORAGE,
            {}});
  run_loop.Run();

  ASSERT_FALSE(host_->IsMounted());
  EXPECT_FALSE(mojo_bootstrap::PendingConnectionManager::Get().OpenIpcChannel(
      token, {}));
}

TEST_F(DriveFsHostTest, DestroyBeforeMojoConnection) {
  auto token = StartMount();
  CallMountCallbackSuccess(token);

  base::RunLoop run_loop;
  EXPECT_CALL(*disk_manager_, UnmountPath("/media/drivefsroot/salt-g-ID", _))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  host_.reset();
  EXPECT_FALSE(mojo_bootstrap::PendingConnectionManager::Get().OpenIpcChannel(
      token, {}));

  run_loop.Run();
}

TEST_F(DriveFsHostTest, MountWhileAlreadyMounted) {
  DoMount();
  EXPECT_FALSE(host_->Mount());
}

TEST_F(DriveFsHostTest, UnsupportedAccountTypes) {
  EXPECT_CALL(*disk_manager_, MountPath(_, _, _, _, _, _, _)).Times(0);
  const AccountId unsupported_accounts[] = {
      AccountId::FromGaiaId("ID"),
      AccountId::FromUserEmail("test2@example.com"),
      AccountId::AdFromObjGuid("ID"),
  };
  for (auto& account : unsupported_accounts) {
    host_delegate_ = std::make_unique<TestingDriveFsHostDelegate>(
        identity_test_env_.identity_manager(), account);
    host_ = std::make_unique<DriveFsHost>(
        profile_path_, host_delegate_.get(), host_delegate_.get(),
        network_connection_tracker_.get(), &clock_, disk_manager_.get(),
        std::make_unique<base::MockOneShotTimer>());
    EXPECT_FALSE(host_->Mount());
    EXPECT_FALSE(host_->IsMounted());
  }
}

TEST_F(DriveFsHostTest, GetAccessToken_UnmountDuringMojoRequest) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  base::RunLoop run_loop;
  delegate_.set_disconnect_handler(run_loop.QuitClosure());
  delegate_->GetAccessToken(
      "client ID", "app ID", {"scope1", "scope2"},
      base::BindLambdaForTesting([](mojom::AccessTokenStatus status,
                                    const std::string& token) { FAIL(); }));
  host_->Unmount();
  run_loop.Run();
  EXPECT_FALSE(host_->IsMounted());

  EXPECT_FALSE(identity_test_env_.IsAccessTokenRequestPending());
}

ACTION_P(CloneStruct, output) {
  *output = arg0.Clone();
}

TEST_F(DriveFsHostTest, OnSyncingStatusUpdate_ForwardToObservers) {
  ASSERT_NO_FATAL_FAILURE(DoMount());
  MockDriveFsHostObserver observer;
  base::ScopedObservation<DriveFsHost, DriveFsHostObserver> observation_scoper(
      &observer);
  observation_scoper.Observe(host_.get());
  auto status = mojom::SyncingStatus::New();
  status->item_events.emplace_back(absl::in_place, 12, 34, "filename.txt",
                                   mojom::ItemEvent::State::kInProgress, 123,
                                   456, mojom::ItemEventReason::kPin);
  mojom::SyncingStatusPtr observed_status;
  EXPECT_CALL(observer, OnSyncingStatusUpdate(_))
      .WillOnce(CloneStruct(&observed_status));
  delegate_->OnSyncingStatusUpdate(status.Clone());
  delegate_.FlushForTesting();
  testing::Mock::VerifyAndClear(&observer);

  EXPECT_EQ(status, observed_status);
}

ACTION_P(CloneVectorOfStructs, output) {
  for (auto& s : arg0) {
    output->emplace_back(s.Clone());
  }
}

TEST_F(DriveFsHostTest, OnFilesChanged_ForwardToObservers) {
  ASSERT_NO_FATAL_FAILURE(DoMount());
  MockDriveFsHostObserver observer;
  base::ScopedObservation<DriveFsHost, DriveFsHostObserver> observation_scoper(
      &observer);
  observation_scoper.Observe(host_.get());
  std::vector<mojom::FileChangePtr> changes;
  changes.emplace_back(absl::in_place, base::FilePath("/create"),
                       mojom::FileChange::Type::kCreate);
  changes.emplace_back(absl::in_place, base::FilePath("/delete"),
                       mojom::FileChange::Type::kDelete);
  changes.emplace_back(absl::in_place, base::FilePath("/modify"),
                       mojom::FileChange::Type::kModify);
  std::vector<mojom::FileChangePtr> observed_changes;
  EXPECT_CALL(observer, OnFilesChanged(_))
      .WillOnce(CloneVectorOfStructs(&observed_changes));
  delegate_->OnFilesChanged(mojo::Clone(changes));
  delegate_.FlushForTesting();
  testing::Mock::VerifyAndClear(&observer);

  EXPECT_EQ(changes, observed_changes);
}

TEST_F(DriveFsHostTest, OnError_ForwardToObservers) {
  ASSERT_NO_FATAL_FAILURE(DoMount());
  MockDriveFsHostObserver observer;
  base::ScopedObservation<DriveFsHost, DriveFsHostObserver> observation_scoper(
      &observer);
  observation_scoper.Observe(host_.get());
  auto error = mojom::DriveError::New(
      mojom::DriveError::Type::kCantUploadStorageFull, base::FilePath("/foo"));
  mojom::DriveErrorPtr observed_error;
  EXPECT_CALL(observer, OnError(_)).WillOnce(CloneStruct(&observed_error));
  delegate_->OnError(error.Clone());
  delegate_.FlushForTesting();
  testing::Mock::VerifyAndClear(&observer);

  EXPECT_EQ(error, observed_error);
}

TEST_F(DriveFsHostTest, OnError_IgnoreUnknownErrorTypes) {
  ASSERT_NO_FATAL_FAILURE(DoMount());
  MockDriveFsHostObserver observer;
  base::ScopedObservation<DriveFsHost, DriveFsHostObserver> observation_scoper(
      &observer);
  observation_scoper.Observe(host_.get());
  EXPECT_CALL(observer, OnError(_)).Times(0);
  delegate_->OnError(mojom::DriveError::New(
      static_cast<mojom::DriveError::Type>(
          static_cast<std::underlying_type_t<mojom::DriveError::Type>>(
              mojom::DriveError::Type::kMaxValue) +
          1),
      base::FilePath("/foo")));
  delegate_.FlushForTesting();
}

TEST_F(DriveFsHostTest, DisplayConfirmDialog_ForwardToHandler) {
  ASSERT_NO_FATAL_FAILURE(DoMount());
  auto reason = mojom::DialogReason::New(
      mojom::DialogReason::Type::kEnableDocsOffline, base::FilePath());
  mojom::DialogReasonPtr observed_reason;
  host_->set_dialog_handler(base::BindLambdaForTesting(
      [&](const mojom::DialogReason& reason,
          base::OnceCallback<void(mojom::DialogResult)> callback) {
        observed_reason = reason.Clone();
        std::move(callback).Run(mojom::DialogResult::kAccept);
      }));
  bool called = false;
  delegate_->DisplayConfirmDialog(
      reason.Clone(),
      base::BindLambdaForTesting([&](mojom::DialogResult result) {
        EXPECT_EQ(mojom::DialogResult::kAccept, result);
        called = true;
      }));
  delegate_.FlushForTesting();
  EXPECT_EQ(reason, observed_reason);
  EXPECT_TRUE(called);
}

TEST_F(DriveFsHostTest, DisplayConfirmDialogImpl_IgnoreIfNoHandler) {
  ASSERT_NO_FATAL_FAILURE(DoMount());
  bool called = false;
  delegate_->DisplayConfirmDialog(
      mojom::DialogReason::New(mojom::DialogReason::Type::kEnableDocsOffline,
                               base::FilePath()),
      base::BindLambdaForTesting([&](mojom::DialogResult result) {
        EXPECT_EQ(mojom::DialogResult::kNotDisplayed, result);
        called = true;
      }));
  delegate_.FlushForTesting();
  EXPECT_TRUE(called);
}

TEST_F(DriveFsHostTest, DisplayConfirmDialogImpl_IgnoreUnknownReasonTypes) {
  ASSERT_NO_FATAL_FAILURE(DoMount());
  host_->set_dialog_handler(base::BindRepeating(
      [](const mojom::DialogReason&,
         base::OnceCallback<void(mojom::DialogResult)>) { NOTREACHED(); }));
  bool called = false;
  delegate_->DisplayConfirmDialog(
      mojom::DialogReason::New(
          static_cast<mojom::DialogReason::Type>(
              static_cast<std::underlying_type_t<mojom::DialogReason::Type>>(
                  mojom::DialogReason::Type::kMaxValue) +
              1),
          base::FilePath()),
      base::BindLambdaForTesting([&](mojom::DialogResult result) {
        EXPECT_EQ(mojom::DialogResult::kNotDisplayed, result);
        called = true;
      }));
  delegate_.FlushForTesting();
  EXPECT_TRUE(called);
}

TEST_F(DriveFsHostTest, TeamDriveTracking) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  delegate_->OnTeamDrivesListReady({"a", "b"});
  delegate_.FlushForTesting();
  EXPECT_EQ(
      (std::set<std::string>{"a", "b"}),
      host_delegate_->GetDriveNotificationManager().team_drive_ids_for_test());

  delegate_->OnTeamDriveChanged(
      "c", mojom::DriveFsDelegate::CreateOrDelete::kCreated);
  delegate_.FlushForTesting();
  EXPECT_EQ(
      (std::set<std::string>{"a", "b", "c"}),
      host_delegate_->GetDriveNotificationManager().team_drive_ids_for_test());

  delegate_->OnTeamDriveChanged(
      "b", mojom::DriveFsDelegate::CreateOrDelete::kDeleted);
  delegate_.FlushForTesting();
  EXPECT_EQ(
      (std::set<std::string>{"a", "c"}),
      host_delegate_->GetDriveNotificationManager().team_drive_ids_for_test());
}

TEST_F(DriveFsHostTest, TeamDriveTrackingIgnoreChanges) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  EXPECT_EQ(
      std::set<std::string>(),
      host_delegate_->GetDriveNotificationManager().team_drive_ids_for_test());

  delegate_->OnTeamDriveChanged(
      "a", mojom::DriveFsDelegate::CreateOrDelete::kCreated);
  delegate_.FlushForTesting();
  EXPECT_EQ(
      std::set<std::string>(),
      host_delegate_->GetDriveNotificationManager().team_drive_ids_for_test());

  delegate_->OnTeamDriveChanged(
      "b", mojom::DriveFsDelegate::CreateOrDelete::kDeleted);
  delegate_.FlushForTesting();
  EXPECT_EQ(
      std::set<std::string>(),
      host_delegate_->GetDriveNotificationManager().team_drive_ids_for_test());
}

TEST_F(DriveFsHostTest, Invalidation) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  delegate_->OnTeamDrivesListReady({"a", "b"});
  delegate_.FlushForTesting();

  EXPECT_CALL(mock_drivefs_,
              FetchChangeLogImpl(std::vector<std::pair<int64_t, std::string>>{
                  {123, ""}, {456, "a"}}));

  for (auto& observer :
       host_delegate_->GetDriveNotificationManager().observers_for_test()) {
    observer.OnNotificationReceived({{"", 123}, {"a", 456}});
  }
  receiver_.FlushForTesting();
}

TEST_F(DriveFsHostTest, InvalidateAll) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  delegate_->OnTeamDrivesListReady({"a", "b"});
  delegate_.FlushForTesting();

  EXPECT_CALL(mock_drivefs_, FetchAllChangeLogs());

  for (auto& observer :
       host_delegate_->GetDriveNotificationManager().observers_for_test()) {
    observer.OnNotificationTimerFired();
  }
  receiver_.FlushForTesting();
}

TEST_F(DriveFsHostTest, RemoveDriveNotificationObserver) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  delegate_->OnTeamDrivesListReady({"a", "b"});
  delegate_.FlushForTesting();
  EXPECT_TRUE(!host_delegate_->GetDriveNotificationManager()
                   .observers_for_test()
                   .empty());

  host_.reset();

  EXPECT_FALSE(!host_delegate_->GetDriveNotificationManager()
                    .observers_for_test()
                    .empty());
}

TEST_F(DriveFsHostTest, Remount_CachedOnceOnly) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  // Request an access token.
  delegate_->GetAccessToken(
      "client ID", "app ID", {"scope1", "scope2"},
      base::BindLambdaForTesting(
          [&](mojom::AccessTokenStatus status, const std::string& token) {
            EXPECT_EQ(mojom::AccessTokenStatus::kSuccess, status);
            EXPECT_EQ("auth token", token);
          }));
  delegate_.FlushForTesting();
  EXPECT_TRUE(identity_test_env_.IsAccessTokenRequestPending());

  // Fulfill the request.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "auth token", clock_.Now() + kTokenLifetime);
  EXPECT_FALSE(identity_test_env_.IsAccessTokenRequestPending());

  absl::optional<base::TimeDelta> delay = base::Seconds(5);
  EXPECT_CALL(*host_delegate_, OnUnmounted(delay));
  SendOnUnmounted(delay);
  base::RunLoop().RunUntilIdle();
  ASSERT_NO_FATAL_FAILURE(DoUnmount());

  // Second mount attempt should reuse already available token.
  ASSERT_NO_FATAL_FAILURE(DoMount());
  EXPECT_FALSE(identity_test_env_.IsAccessTokenRequestPending());
  EXPECT_EQ("auth token", init_access_token_.value_or(""));

  // But if it asks for token again it goes to identity manager.
  delegate_->GetAccessToken(
      "client ID", "app ID", {"scope1", "scope2"},
      base::BindLambdaForTesting(
          [&](mojom::AccessTokenStatus status, const std::string& token) {
            EXPECT_EQ(mojom::AccessTokenStatus::kSuccess, status);
            EXPECT_EQ("auth token 2", token);
          }));
  delegate_.FlushForTesting();
  EXPECT_TRUE(identity_test_env_.IsAccessTokenRequestPending());

  // Fulfill the request with a different token.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "auth token 2", clock_.Now() + kTokenLifetime);
  EXPECT_FALSE(identity_test_env_.IsAccessTokenRequestPending());
}

TEST_F(DriveFsHostTest, Remount_RequestInflight) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  delegate_->GetAccessToken(
      "client ID", "app ID", {"scope1", "scope2"},
      base::BindLambdaForTesting([&](mojom::AccessTokenStatus status,
                                     const std::string& token) { FAIL(); }));

  absl::optional<base::TimeDelta> delay = base::Seconds(5);
  EXPECT_CALL(*host_delegate_, OnUnmounted(delay));
  SendOnUnmounted(delay);
  base::RunLoop().RunUntilIdle();
  ASSERT_NO_FATAL_FAILURE(DoUnmount());
  EXPECT_TRUE(identity_test_env_.IsAccessTokenRequestPending());

  // Now the response is ready.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "auth token", clock_.Now() + kTokenLifetime);

  // Second mount will reuse previous token.
  ASSERT_NO_FATAL_FAILURE(DoMount());
  EXPECT_FALSE(identity_test_env_.IsAccessTokenRequestPending());
  EXPECT_EQ("auth token", init_access_token_.value_or(""));
}

TEST_F(DriveFsHostTest, Remount_RequestInflightCompleteAfterMount) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  delegate_->GetAccessToken(
      "client ID", "app ID", {"scope1", "scope2"},
      base::BindLambdaForTesting([&](mojom::AccessTokenStatus status,
                                     const std::string& token) { FAIL(); }));

  absl::optional<base::TimeDelta> delay = base::Seconds(5);
  EXPECT_CALL(*host_delegate_, OnUnmounted(delay));
  SendOnUnmounted(delay);
  base::RunLoop().RunUntilIdle();
  ASSERT_NO_FATAL_FAILURE(DoUnmount());
  EXPECT_TRUE(identity_test_env_.IsAccessTokenRequestPending());

  // Second mount will reuse previous token.
  ASSERT_NO_FATAL_FAILURE(DoMount());
  EXPECT_FALSE(init_access_token_);
  EXPECT_TRUE(identity_test_env_.IsAccessTokenRequestPending());

  // Now the response is ready.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "auth token", clock_.Now() + kTokenLifetime);
  EXPECT_FALSE(identity_test_env_.IsAccessTokenRequestPending());

  // A new request will reuse the cached token.
  delegate_->GetAccessToken(
      "client ID", "app ID", {"scope1", "scope2"},
      base::BindLambdaForTesting(
          [&](mojom::AccessTokenStatus status, const std::string& token) {
            EXPECT_EQ(mojom::AccessTokenStatus::kSuccess, status);
            EXPECT_EQ("auth token", token);
          }));
  delegate_.FlushForTesting();
  EXPECT_FALSE(identity_test_env_.IsAccessTokenRequestPending());
}

TEST_F(DriveFsHostTest, ConnectToExtension) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  mojo::Remote<drivefs::mojom::NativeMessagingPort> remote;
  mojo::PendingRemote<drivefs::mojom::NativeMessagingHost> host_remote;
  auto receiver = host_remote.InitWithNewPipeAndPassReceiver();

  base::RunLoop run_loop;
  delegate_->ConnectToExtension(
      drivefs::mojom::ExtensionConnectionParams::New("foo"),
      remote.BindNewPipeAndPassReceiver(), std::move(host_remote),
      base::BindLambdaForTesting(
          [&](drivefs::mojom::DriveFsDelegate::ExtensionConnectionStatus
                  status) {
            EXPECT_EQ(drivefs::mojom::DriveFsDelegate::
                          ExtensionConnectionStatus::kExtensionNotFound,
                      status);
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_EQ("foo", host_delegate_->get_last_extension_params().extension_id);
}

}  // namespace
}  // namespace drivefs
