// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_session_impl.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>
#include <optional>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/session/arc_client_adapter.h"
#include "ash/components/arc/session/arc_start_params.h"
#include "ash/components/arc/session/arc_upgrade_params.h"
#include "ash/components/arc/session/mojo_invitation_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_arc_bridge_host.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/system/scheduler_configuration_manager_base.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cryptohome {
class Identification;
}  // namespace cryptohome

namespace arc {
namespace {

constexpr char kDefaultLocale[] = "en-US";

UpgradeParams DefaultUpgradeParams() {
  UpgradeParams params;
  params.locale = kDefaultLocale;
  return params;
}

std::string ConvertToString(ArcSessionImpl::State state) {
  std::stringstream ss;
  ss << state;
  return ss.str();
}

// An ArcClientAdapter implementation that does the same as the real ones but
// without any D-Bus calls.
class FakeArcClientAdapter : public ArcClientAdapter {
 public:
  FakeArcClientAdapter() = default;
  ~FakeArcClientAdapter() override = default;

  FakeArcClientAdapter(const FakeArcClientAdapter&) = delete;
  FakeArcClientAdapter& operator=(const FakeArcClientAdapter&) = delete;

  // ArcClientAdapter overrides:
  void StartMiniArc(StartParams params,
                    chromeos::VoidDBusMethodCallback callback) override {
    last_start_params_ = std::move(params);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FakeArcClientAdapter::OnMiniArcStarted,
                                  base::Unretained(this), std::move(callback),
                                  arc_available_));
  }

  void UpgradeArc(UpgradeParams params,
                  chromeos::VoidDBusMethodCallback callback) override {
    last_upgrade_params_ = std::move(params);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FakeArcClientAdapter::OnArcUpgraded,
                                  base::Unretained(this), std::move(callback),
                                  !force_upgrade_failure_));
  }

  void StopArcInstance(bool on_shutdown, bool should_backup_log) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeArcClientAdapter::NotifyArcInstanceStopped,
                       base::Unretained(this), false /* is_system_shutdown */));
  }

  void SetUserInfo(const cryptohome::Identification& cryptohome_id,
                   const std::string& hash,
                   const std::string& serial_number) override {}

  void SetDemoModeDelegate(DemoModeDelegate* delegate) override {}
  void TrimVmMemory(TrimVmMemoryCallback callback, int page_limit) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true, std::string()));
  }

  // Notifies ArcSessionImpl of the ARC instance stop event.
  void NotifyArcInstanceStopped(bool is_system_shutdown) {
    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped(is_system_shutdown);
  }

  void set_arc_available(bool arc_available) { arc_available_ = arc_available; }
  void set_force_upgrade_failure(bool force_upgrade_failure) {
    force_upgrade_failure_ = force_upgrade_failure;
  }
  const StartParams& last_start_params() const { return last_start_params_; }
  const UpgradeParams& last_upgrade_params() const {
    return last_upgrade_params_;
  }

 private:
  void OnMiniArcStarted(chromeos::VoidDBusMethodCallback callback,
                        bool result) {
    std::move(callback).Run(result);
  }

  void OnArcUpgraded(chromeos::VoidDBusMethodCallback callback, bool result) {
    std::move(callback).Run(result);
    if (!result) {
      NotifyArcInstanceStopped(false /* is_system_shutdown */);
    }
  }

  bool arc_available_ = true;
  bool force_upgrade_failure_ = false;
  StartParams last_start_params_;
  UpgradeParams last_upgrade_params_;
};

class FakeDelegate : public ArcSessionImpl::Delegate {
 public:
  FakeDelegate() = default;

  FakeDelegate(const FakeDelegate&) = delete;
  FakeDelegate& operator=(const FakeDelegate&) = delete;

  // Emulates to fail Mojo connection establishing. |callback| passed to
  // ConnectMojo will be called with nullptr.
  void EmulateMojoConnectionFailure() { success_ = false; }

  // Suspends to complete the MojoConnection, when ConnectMojo is called.
  // Later, when ResumeMojoConnection() is called, the passed callback will be
  // asynchronously called.
  void SuspendMojoConnection() { suspend_ = true; }

  // Resumes the pending Mojo connection establishment. Before,
  // SuspendMojoConnection() must be called followed by ConnectMojo().
  // ConnectMojo's |callback| will be called asynchronously.
  void ResumeMojoConnection() {
    DCHECK(!pending_callback_.is_null());
    PostCallback(std::move(pending_callback_));
  }

  // ArcSessionImpl::Delegate overrides:
  void CreateSocket(CreateSocketCallback callback) override {
    // Open /dev/null as a dummy FD.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  base::ScopedFD(open("/dev/null",
                                                      O_RDONLY | O_CLOEXEC))));
  }

  base::ScopedFD ConnectMojo(base::ScopedFD socket_fd,
                             ConnectMojoCallback callback) override {
    if (suspend_) {
      DCHECK(pending_callback_.is_null());
      pending_callback_ = std::move(callback);
    } else {
      PostCallback(std::move(callback));
    }

    // Open /dev/null as a dummy FD.
    return base::ScopedFD(open("/dev/null", O_RDONLY | O_CLOEXEC));
  }

  void GetFreeDiskSpace(GetFreeDiskSpaceCallback callback) override {
    std::move(callback).Run(free_disk_space_);
  }

  version_info::Channel GetChannel() override {
    return version_info::Channel::DEFAULT;
  }

  std::unique_ptr<ArcClientAdapter> CreateClient() override {
    return std::make_unique<FakeArcClientAdapter>();
  }

  void SetFreeDiskSpace(int64_t space) { free_disk_space_ = space; }

 private:
  void PostCallback(ConnectMojoCallback callback) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            success_ ? std::make_unique<FakeArcBridgeHost>() : nullptr,
            success_ ? std::make_unique<MojoInvitationManager>() : nullptr));
  }

  bool success_ = true;
  bool suspend_ = false;
  int64_t free_disk_space_ = kMinimumFreeDiskSpaceBytes * 2;
  ConnectMojoCallback pending_callback_;
};

class TestArcSessionObserver : public ArcSession::Observer {
 public:
  struct OnSessionStoppedArgs {
    ArcStopReason reason;
    bool was_running;
    bool upgrade_requested;
  };

  explicit TestArcSessionObserver(ArcSession* arc_session)
      : arc_session_(arc_session) {
    arc_session_->AddObserver(this);
  }
  TestArcSessionObserver(ArcSession* arc_session, base::RunLoop* run_loop)
      : arc_session_(arc_session), run_loop_(run_loop) {
    arc_session_->AddObserver(this);
  }

  TestArcSessionObserver(const TestArcSessionObserver&) = delete;
  TestArcSessionObserver& operator=(const TestArcSessionObserver&) = delete;

  ~TestArcSessionObserver() override { arc_session_->RemoveObserver(this); }

  const std::optional<OnSessionStoppedArgs>& on_session_stopped_args() const {
    return on_session_stopped_args_;
  }

  // ArcSession::Observer overrides:
  void OnSessionStopped(ArcStopReason reason,
                        bool was_running,
                        bool upgrade_requested) override {
    on_session_stopped_args_.emplace(
        OnSessionStoppedArgs{reason, was_running, upgrade_requested});
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

 private:
  const raw_ptr<ArcSession> arc_session_;            // Not owned.
  const raw_ptr<base::RunLoop> run_loop_ = nullptr;  // Not owned.
  std::optional<OnSessionStoppedArgs> on_session_stopped_args_;
};

// Custom deleter for ArcSession testing.
struct ArcSessionDeleter {
  void operator()(ArcSession* arc_session) {
    // ArcSessionImpl must be in STOPPED state, if the instance is being
    // destroyed. Calling OnShutdown() just before ensures it.
    arc_session->OnShutdown();
    delete arc_session;
  }
};

class FakeSchedulerConfigurationManager
    : public ash::SchedulerConfigurationManagerBase {
 public:
  FakeSchedulerConfigurationManager() = default;

  FakeSchedulerConfigurationManager(const FakeSchedulerConfigurationManager&) =
      delete;
  FakeSchedulerConfigurationManager& operator=(
      const FakeSchedulerConfigurationManager&) = delete;

  ~FakeSchedulerConfigurationManager() override = default;

  void SetLastReply(size_t num_cores_disabled) {
    reply_ = std::make_pair(true, num_cores_disabled);
    for (Observer& obs : observer_list_)
      obs.OnConfigurationSet(reply_->first, reply_->second);
  }

  std::optional<std::pair<bool, size_t>> GetLastReply() const override {
    return reply_;
  }

 private:
  std::optional<std::pair<bool, size_t>> reply_;
};

class FakeAdbSideloadingAvailabilityDelegate
    : public AdbSideloadingAvailabilityDelegate {
 public:
  FakeAdbSideloadingAvailabilityDelegate() = default;
  ~FakeAdbSideloadingAvailabilityDelegate() override = default;

  void CanChangeAdbSideloading(
      base::OnceCallback<void(bool can_change_adb_sideloading)> callback)
      override {
    std::move(callback).Run(can_change_adb_sideloading_);
  }

  void SetCanChangeAdbSideloading(bool can_change) {
    can_change_adb_sideloading_ = can_change;
  }

 private:
  bool can_change_adb_sideloading_ = false;
};

class ArcSessionImplTest : public testing::Test {
 public:
  ArcSessionImplTest() = default;

  ArcSessionImplTest(const ArcSessionImplTest&) = delete;
  ArcSessionImplTest& operator=(const ArcSessionImplTest&) = delete;

  ~ArcSessionImplTest() override = default;

  std::unique_ptr<ArcSessionImpl, ArcSessionDeleter> CreateArcSession(
      std::unique_ptr<ArcSessionImpl::Delegate> delegate = nullptr,
      float default_device_scale_factor = 1.0f) {
    auto arc_session = CreateArcSessionInternal(std::move(delegate),
                                                default_device_scale_factor);
    fake_schedule_configuration_manager_.SetLastReply(0);
    return arc_session;
  }

  std::unique_ptr<ArcSessionImpl, ArcSessionDeleter>
  CreateArcSessionWithoutCpuInfo(
      std::unique_ptr<ArcSessionImpl::Delegate> delegate = nullptr,
      float default_device_scale_factor = 1.0f) {
    return CreateArcSessionInternal(std::move(delegate),
                                    default_device_scale_factor);
  }

  void SetupMiniContainer(ArcSessionImpl* arc_session,
                          TestArcSessionObserver* observer) {
    arc_session->StartMiniInstance();
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(ArcSessionImpl::State::RUNNING_MINI_INSTANCE,
              arc_session->GetStateForTesting());
    ASSERT_FALSE(observer->on_session_stopped_args().has_value());
  }

 protected:
  FakeArcClientAdapter* GetClient(ArcSessionImpl* session) {
    return static_cast<FakeArcClientAdapter*>(session->GetClientForTesting());
  }

  FakeSchedulerConfigurationManager fake_schedule_configuration_manager_;

  std::unique_ptr<FakeAdbSideloadingAvailabilityDelegate>
      adb_sideloading_availability_delegate_ =
          std::make_unique<FakeAdbSideloadingAvailabilityDelegate>();

 private:
  std::unique_ptr<ArcSessionImpl, ArcSessionDeleter> CreateArcSessionInternal(
      std::unique_ptr<ArcSessionImpl::Delegate> delegate,
      float default_device_scale_factor) {
    if (!delegate) {
      delegate = std::make_unique<FakeDelegate>();
    }
    auto arc_session =
        std::unique_ptr<ArcSessionImpl, ArcSessionDeleter>(new ArcSessionImpl(
            std::move(delegate), &fake_schedule_configuration_manager_,
            adb_sideloading_availability_delegate_.get()));
    arc_session->SetDefaultDeviceScaleFactor(default_device_scale_factor);
    return arc_session;
  }

  base::test::TaskEnvironment task_environment_;
};

// Starting mini container success case.
TEST_F(ArcSessionImplTest, MiniInstance_Success) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->StartMiniInstance();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::RUNNING_MINI_INSTANCE,
            arc_session->GetStateForTesting());
  EXPECT_FALSE(observer.on_session_stopped_args().has_value());
}

// ArcClientAdapter::StartMiniArc() reports an error, causing the mini instance
// start to fail.
TEST_F(ArcSessionImplTest, MiniInstance_DBusFail) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  GetClient(arc_session.get())->set_arc_available(false);
  arc_session->StartMiniInstance();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_FALSE(observer.on_session_stopped_args()->upgrade_requested);
}

// ArcClientAdapter::UpgradeArc() reports an error due to low disk,
// causing the container upgrade to fail to start container with reason
// LOW_DISK_SPACE.
TEST_F(ArcSessionImplTest, Upgrade_LowDisk) {
  auto delegate = std::make_unique<FakeDelegate>();
  delegate->SetFreeDiskSpace(kMinimumFreeDiskSpaceBytes / 2);

  // Set up. Start mini-container. The mini-container doesn't use the disk, so
  // there being low disk space won't cause it to start.
  auto arc_session = CreateArcSession(std::move(delegate));

  base::RunLoop run_loop;
  TestArcSessionObserver observer(arc_session.get(), &run_loop);
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  arc_session->RequestUpgrade(DefaultUpgradeParams());
  run_loop.Run();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::LOW_DISK_SPACE,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_TRUE(observer.on_session_stopped_args()->upgrade_requested);
}

// Upgrading a mini container to a full container. Success case.
TEST_F(ArcSessionImplTest, Upgrade_Success) {
  // Set up. Start a mini instance.
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // Then, upgrade to a full instance.
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::RUNNING_FULL_INSTANCE,
            arc_session->GetStateForTesting());
  EXPECT_FALSE(observer.on_session_stopped_args().has_value());
}

// ArcClientAdapter::UpgradeArc() reports an error, then the upgrade fails.
TEST_F(ArcSessionImplTest, Upgrade_DBusFail) {
  // Set up. Start a mini instance.
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // Hereafter, let ArcClientAdapter::UpgradeArc() fail.
  GetClient(arc_session.get())->set_force_upgrade_failure(true);

  // Then upgrade, which should fail.
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_TRUE(observer.on_session_stopped_args()->upgrade_requested);
}

// Mojo connection fails on upgrading. Then, the upgrade fails.
TEST_F(ArcSessionImplTest, Upgrade_MojoConnectionFail) {
  // Let Mojo connection fail.
  auto delegate = std::make_unique<FakeDelegate>();
  delegate->EmulateMojoConnectionFailure();

  // Set up. Start mini instance.
  auto arc_session = CreateArcSession(std::move(delegate));
  TestArcSessionObserver observer(arc_session.get());
  // Starting mini instance should succeed, because it is not related to
  // Mojo connection.
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // Upgrade should fail, due to Mojo connection fail set above.
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_TRUE(observer.on_session_stopped_args()->upgrade_requested);
}

// Calling UpgradeArcContainer() during STARTING_MINI_INSTANCE should eventually
// succeed to run a full container.
TEST_F(ArcSessionImplTest, Upgrade_StartingMiniInstance) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->StartMiniInstance();
  ASSERT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  // Before moving forward to RUNNING_MINI_INSTANCE, start upgrading it.
  arc_session->RequestUpgrade(DefaultUpgradeParams());

  // The state should not immediately switch to STARTING_FULL_INSTANCE, yet.
  EXPECT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  // Complete the upgrade procedure.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::RUNNING_FULL_INSTANCE,
            arc_session->GetStateForTesting());
  EXPECT_FALSE(observer.on_session_stopped_args().has_value());
}

// Testing stop during START_MINI_INSTANCE.
TEST_F(ArcSessionImplTest, Stop_StartingMiniInstance) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->StartMiniInstance();
  ASSERT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_FALSE(observer.on_session_stopped_args()->upgrade_requested);
}

// Testing stop during RUNNING_MINI_INSTANCE.
TEST_F(ArcSessionImplTest, Stop_RunningMiniInstance) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->StartMiniInstance();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionImpl::State::RUNNING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_FALSE(observer.on_session_stopped_args()->upgrade_requested);
}

// Testing stop during STARTING_FULL_INSTANCE for upgrade.
TEST_F(ArcSessionImplTest, Stop_StartingFullInstanceForUpgrade) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  // Start mini container.
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // Then upgrade.
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  ASSERT_EQ(ArcSessionImpl::State::STARTING_FULL_INSTANCE,
            arc_session->GetStateForTesting());

  // Request to stop during STARTING_FULL_INSTANCE state.
  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_TRUE(observer.on_session_stopped_args()->upgrade_requested);
}

// Testing stop during CONNECTING_MOJO for upgrade.
TEST_F(ArcSessionImplTest, Stop_ConnectingMojoForUpgrade) {
  // Let Mojo connection suspend.
  auto delegate = std::make_unique<FakeDelegate>();
  delegate->SuspendMojoConnection();
  auto* delegate_ptr = delegate.get();
  auto arc_session = CreateArcSession(std::move(delegate));
  TestArcSessionObserver observer(arc_session.get());
  // Start mini container.
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // Then upgrade. This should suspend at Mojo connection.
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionImpl::State::CONNECTING_MOJO,
            arc_session->GetStateForTesting());

  // Request to stop, then resume the Mojo connection.
  arc_session->Stop();
  delegate_ptr->ResumeMojoConnection();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_TRUE(observer.on_session_stopped_args()->upgrade_requested);
}

// Testing stop during RUNNING_FULL_INSTANCE after upgrade.
TEST_F(ArcSessionImplTest, Stop_RunningFullInstanceForUpgrade) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  // Start mini container.
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // And upgrade successfully.
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionImpl::State::RUNNING_FULL_INSTANCE,
            arc_session->GetStateForTesting());

  // Then request to stop.
  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_TRUE(observer.on_session_stopped_args()->was_running);
  EXPECT_TRUE(observer.on_session_stopped_args()->upgrade_requested);
}

// Testing stop during STARTING_MINI_INSTANCE with upgrade request.
TEST_F(ArcSessionImplTest,
       Stop_StartingFullInstanceForUpgradeDuringMiniInstanceStart) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->StartMiniInstance();
  ASSERT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  // Request to upgrade during starting mini container.
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  // Then, the state should stay at STARTING_MINI_INSTANCE.
  ASSERT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  // Request to stop.
  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_TRUE(observer.on_session_stopped_args()->upgrade_requested);
}

// Emulating crash.
TEST_F(ArcSessionImplTest, ArcStopInstance) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->StartMiniInstance();
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionImpl::State::RUNNING_FULL_INSTANCE,
            arc_session->GetStateForTesting());

  // Notify ArcClientAdapter's observers of the crash event.
  GetClient(arc_session.get())
      ->NotifyArcInstanceStopped(false /* is_system_shutdown */);

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::CRASH, observer.on_session_stopped_args()->reason);
  EXPECT_TRUE(observer.on_session_stopped_args()->was_running);
  EXPECT_TRUE(observer.on_session_stopped_args()->upgrade_requested);
}

// Emulating system shutdown.
TEST_F(ArcSessionImplTest, ArcStopInstanceSystemShutdown) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->StartMiniInstance();
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionImpl::State::RUNNING_FULL_INSTANCE,
            arc_session->GetStateForTesting());

  // Notify ArcClientAdapter's observers of the shutdown event.
  GetClient(arc_session.get())
      ->NotifyArcInstanceStopped(true /* is_system_shutdown */);

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_TRUE(observer.on_session_stopped_args()->was_running);
  EXPECT_TRUE(observer.on_session_stopped_args()->upgrade_requested);
}

struct PackagesCacheModeState {
  // Possible values for ash::switches::kArcPackagesCacheMode
  const char* chrome_switch;
  bool full_container;
  UpgradeParams::PackageCacheMode expected_packages_cache_mode;
};

constexpr PackagesCacheModeState kPackagesCacheModeStates[] = {
    {nullptr, true, UpgradeParams::PackageCacheMode::DEFAULT},
    {nullptr, false, UpgradeParams::PackageCacheMode::DEFAULT},
    {kPackagesCacheModeCopy, true,
     UpgradeParams::PackageCacheMode::COPY_ON_INIT},
    {kPackagesCacheModeCopy, false, UpgradeParams::PackageCacheMode::DEFAULT},
    {kPackagesCacheModeSkipCopy, true,
     UpgradeParams::PackageCacheMode::SKIP_SETUP_COPY_ON_INIT},
    {kPackagesCacheModeCopy, false, UpgradeParams::PackageCacheMode::DEFAULT},
};

class ArcSessionImplPackagesCacheModeTest
    : public ArcSessionImplTest,
      public ::testing::WithParamInterface<PackagesCacheModeState> {};

TEST_P(ArcSessionImplPackagesCacheModeTest, PackagesCacheModes) {
  auto arc_session = CreateArcSession();

  const PackagesCacheModeState& state = GetParam();
  if (state.chrome_switch) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(ash::switches::kArcPackagesCacheMode,
                                    state.chrome_switch);
  }

  arc_session->StartMiniInstance();
  if (state.full_container) {
    arc_session->RequestUpgrade(DefaultUpgradeParams());
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      state.expected_packages_cache_mode,
      GetClient(arc_session.get())->last_upgrade_params().packages_cache_mode);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ArcSessionImplPackagesCacheModeTest,
                         ::testing::ValuesIn(kPackagesCacheModeStates));

class ArcSessionImplGmsCoreCacheTest
    : public ArcSessionImplTest,
      public ::testing::WithParamInterface<bool> {};

TEST_P(ArcSessionImplGmsCoreCacheTest, GmsCoreCaches) {
  auto arc_session = CreateArcSession();

  if (GetParam()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kArcDisableGmsCoreCache);
  }

  arc_session->StartMiniInstance();
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      GetParam(),
      GetClient(arc_session.get())->last_upgrade_params().skip_gms_core_cache);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ArcSessionImplGmsCoreCacheTest,
                         ::testing::Bool());

TEST_F(ArcSessionImplTest, DemoSession) {
  auto arc_session = CreateArcSession();
  arc_session->StartMiniInstance();

  const base::FilePath demo_apps_path(
      "/run/imageloader/demo_mode_resources/android_apps.squash");
  UpgradeParams params;
  params.is_demo_session = true;
  params.demo_session_apps_path = base::FilePath(demo_apps_path);
  params.locale = kDefaultLocale;
  arc_session->RequestUpgrade(std::move(params));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      GetClient(arc_session.get())->last_upgrade_params().is_demo_session);
  EXPECT_EQ(demo_apps_path, GetClient(arc_session.get())
                                ->last_upgrade_params()
                                .demo_session_apps_path);
}

TEST_F(ArcSessionImplTest, DemoSessionWithoutOfflineDemoApps) {
  auto arc_session = CreateArcSession();
  arc_session->StartMiniInstance();

  UpgradeParams params;
  params.is_demo_session = true;
  params.locale = kDefaultLocale;
  arc_session->RequestUpgrade(std::move(params));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      GetClient(arc_session.get())->last_upgrade_params().is_demo_session);
  EXPECT_EQ(base::FilePath(), GetClient(arc_session.get())
                                  ->last_upgrade_params()
                                  .demo_session_apps_path);
}

TEST_F(ArcSessionImplTest, SupervisionTransitionShouldGraduate) {
  auto arc_session = CreateArcSession();
  arc_session->StartMiniInstance();

  UpgradeParams params;
  params.management_transition = ArcManagementTransition::CHILD_TO_REGULAR;
  params.locale = kDefaultLocale;
  arc_session->RequestUpgrade(std::move(params));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ArcManagementTransition::CHILD_TO_REGULAR,
            GetClient(arc_session.get())
                ->last_upgrade_params()
                .management_transition);
  EXPECT_EQ(160, GetClient(arc_session.get())->last_start_params().lcd_density);
}

TEST_F(ArcSessionImplTest, StartArcMiniContainerWithDensity) {
  auto arc_session = CreateArcSessionWithoutCpuInfo(nullptr, 2.f);
  arc_session->StartMiniInstance();
  EXPECT_EQ(ArcSessionImpl::State::WAITING_FOR_NUM_CORES,
            arc_session->GetStateForTesting());
  fake_schedule_configuration_manager_.SetLastReply(2);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::RUNNING_MINI_INSTANCE,
            arc_session->GetStateForTesting());
  EXPECT_EQ(240, GetClient(arc_session.get())->last_start_params().lcd_density);
}

TEST_F(ArcSessionImplTest, StopWhileWaitingForNumCores) {
  auto delegate = std::make_unique<FakeDelegate>();
  auto arc_session = CreateArcSessionWithoutCpuInfo(std::move(delegate));
  arc_session->StartMiniInstance();
  EXPECT_EQ(ArcSessionImpl::State::WAITING_FOR_NUM_CORES,
            arc_session->GetStateForTesting());
  arc_session->Stop();
  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
}

TEST_F(ArcSessionImplTest, ShutdownWhileWaitingForNumCores) {
  auto delegate = std::make_unique<FakeDelegate>();
  auto arc_session = CreateArcSessionWithoutCpuInfo(std::move(delegate));
  arc_session->StartMiniInstance();
  EXPECT_EQ(ArcSessionImpl::State::WAITING_FOR_NUM_CORES,
            arc_session->GetStateForTesting());
  arc_session->OnShutdown();
  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
}

// Test that correct value false for managed sideloading is passed
TEST_F(ArcSessionImplTest, CanChangeAdbSideloading_False) {
  auto arc_session = CreateArcSession();
  adb_sideloading_availability_delegate_->SetCanChangeAdbSideloading(false);

  arc_session->StartMiniInstance();
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(GetClient(arc_session.get())
                   ->last_upgrade_params()
                   .is_managed_adb_sideloading_allowed);
}

// Test that correct value true for managed sideloading is passed
TEST_F(ArcSessionImplTest, CanChangeAdbSideloading_True) {
  auto arc_session = CreateArcSession();
  adb_sideloading_availability_delegate_->SetCanChangeAdbSideloading(true);

  arc_session->StartMiniInstance();
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetClient(arc_session.get())
                  ->last_upgrade_params()
                  .is_managed_adb_sideloading_allowed);
}

// Test that validates arc signed in flag is not set by default.
TEST_F(ArcSessionImplTest, NotArcSignedInByDefault) {
  auto arc_session = CreateArcSession();
  arc_session->StartMiniInstance();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetClient(arc_session.get())->last_start_params().arc_signed_in);
}

// Test that validates use dev caches flag is not set by default.
TEST_F(ArcSessionImplTest, DoNotUseDevCachesByDefault) {
  auto arc_session = CreateArcSession();
  arc_session->StartMiniInstance();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      GetClient(arc_session.get())->last_start_params().use_dev_caches);
}

// Test that validates use dev caches flag is set.
TEST_F(ArcSessionImplTest, UseDevCachesSet) {
  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(ash::switches::kArcUseDevCaches);
  auto arc_session = CreateArcSession();
  arc_session->StartMiniInstance();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetClient(arc_session.get())->last_start_params().use_dev_caches);
}

// Test that validates TTS caching is enabled by default.
TEST_F(ArcSessionImplTest, TTSCachingByDefault) {
  auto arc_session = CreateArcSession();
  arc_session->StartMiniInstance();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      GetClient(arc_session.get())->last_start_params().enable_tts_caching);
}

// Test "<<" operator for ArcSessionImpl::State type.
TEST_F(ArcSessionImplTest, StateTypeStreamOutput) {
  EXPECT_EQ(ConvertToString(ArcSessionImpl::State::NOT_STARTED), "NOT_STARTED");
  EXPECT_EQ(ConvertToString(ArcSessionImpl::State::WAITING_FOR_NUM_CORES),
            "WAITING_FOR_NUM_CORES");
  EXPECT_EQ(ConvertToString(ArcSessionImpl::State::STARTING_MINI_INSTANCE),
            "STARTING_MINI_INSTANCE");
  EXPECT_EQ(ConvertToString(ArcSessionImpl::State::RUNNING_MINI_INSTANCE),
            "RUNNING_MINI_INSTANCE");
  EXPECT_EQ(ConvertToString(ArcSessionImpl::State::STARTING_FULL_INSTANCE),
            "STARTING_FULL_INSTANCE");
  EXPECT_EQ(ConvertToString(ArcSessionImpl::State::CONNECTING_MOJO),
            "CONNECTING_MOJO");
  EXPECT_EQ(ConvertToString(ArcSessionImpl::State::RUNNING_FULL_INSTANCE),
            "RUNNING_FULL_INSTANCE");
  EXPECT_EQ(ConvertToString(ArcSessionImpl::State::STOPPED), "STOPPED");
}
struct DalvikMemoryProfileVariant {
  // Memory stat file
  const char* file_name;
  const StartParams::DalvikMemoryProfile expected_profile;
};

constexpr DalvikMemoryProfileVariant kDalvikMemoryProfileVariant[] = {
    {"non-existing", StartParams::DalvikMemoryProfile::DEFAULT},
    {"2G", StartParams::DalvikMemoryProfile::DEFAULT},
    {"4G", StartParams::DalvikMemoryProfile::M4G},
    {"8G", StartParams::DalvikMemoryProfile::M8G},
    {"16G", StartParams::DalvikMemoryProfile::M16G},
};

class ArcSessionImplDalvikMemoryProfileTest
    : public ArcSessionImplTest,
      public ::testing::WithParamInterface<DalvikMemoryProfileVariant> {};

TEST_P(ArcSessionImplDalvikMemoryProfileTest, DalvikMemoryProfiles) {
  const DalvikMemoryProfileVariant& variant = GetParam();

  auto arc_session = CreateArcSession();
  arc_session->SetSystemMemoryInfoCallbackForTesting(
      base::BindRepeating(&GetSystemMemoryInfoForTesting, variant.file_name));

  arc_session->StartMiniInstance();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      variant.expected_profile,
      GetClient(arc_session.get())->last_start_params().dalvik_memory_profile);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ArcSessionImplDalvikMemoryProfileTest,
                         ::testing::ValuesIn(kDalvikMemoryProfileVariant));

struct HostUreadaheadModeState {
  const char* mode_switch;
  const StartParams::HostUreadaheadMode expected_mode;
};

constexpr HostUreadaheadModeState kHostUreadaheadMode[] = {
    {"readahead", StartParams::HostUreadaheadMode::MODE_READAHEAD},
    {"generate", StartParams::HostUreadaheadMode::MODE_GENERATE},
    {"disabled", StartParams::HostUreadaheadMode::MODE_DISABLED},
};

class ArcSessionImplHostUreadaheadModeTest
    : public ArcSessionImplTest,
      public ::testing::WithParamInterface<HostUreadaheadModeState> {};

TEST_P(ArcSessionImplHostUreadaheadModeTest, HostUreadaheadModes) {
  auto arc_session = CreateArcSession();
  const HostUreadaheadModeState state = GetParam();

  if (state.mode_switch) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(ash::switches::kArcHostUreadaheadMode,
                                    state.mode_switch);
  }

  arc_session->StartMiniInstance();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      state.expected_mode,
      GetClient(arc_session.get())->last_start_params().host_ureadahead_mode);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ArcSessionImplHostUreadaheadModeTest,
                         ::testing::ValuesIn(kHostUreadaheadMode));

}  // namespace
}  // namespace arc
