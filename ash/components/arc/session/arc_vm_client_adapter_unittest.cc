// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/components/arc/session/arc_vm_client_adapter.h"

#include <inttypes.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_dlc_installer.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/arc_session.h"
#include "ash/components/arc/session/file_system_status.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_host.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/posix/safe_strerror.h"
#include "base/process/process_metrics.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/ash/components/dbus/patchpanel/fake_patchpanel_client.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace arc {
namespace {

constexpr const char kArcVmBootNotificationServerAddressPrefix[] =
    "\0test_arcvm_boot_notification_server";

// Disk path contained in CreateDiskImageResponse().
constexpr const char kCreatedDiskImagePath[] = "test/data.img";

constexpr const char kUserIdHash[] = "this_is_a_valid_user_id_hash";
constexpr const char kSerialNumber[] = "AAAABBBBCCCCDDDD1234";
constexpr int64_t kCid = 123;

StartParams GetPopulatedStartParams() {
  StartParams params;
  params.native_bridge_experiment = false;
  params.lcd_density = 240;
  params.arc_file_picker_experiment = true;
  params.play_store_auto_update =
      StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_ON;
  params.arc_custom_tabs_experiment = true;
  params.num_cores_disabled = 2;
  return params;
}

UpgradeParams GetPopulatedUpgradeParams() {
  UpgradeParams params;
  params.account_id = "fee1dead";
  params.skip_boot_completed_broadcast = true;
  params.packages_cache_mode = UpgradeParams::PackageCacheMode::COPY_ON_INIT;
  params.skip_gms_core_cache = true;
  params.management_transition = ArcManagementTransition::CHILD_TO_REGULAR;
  params.locale = "en-US";
  params.preferred_languages = {"en_US", "en", "ja"};
  params.is_demo_session = true;
  params.demo_session_apps_path = base::FilePath("/pato/to/demo.apk");
  return params;
}

vm_tools::concierge::CreateDiskImageResponse CreateDiskImageResponse(
    vm_tools::concierge::DiskImageStatus status) {
  vm_tools::concierge::CreateDiskImageResponse res;
  res.set_status(status);
  res.set_disk_path(base::FilePath(kCreatedDiskImagePath).AsUTF8Unsafe());
  return res;
}

std::string GenerateAbstractAddress() {
  std::string address(kArcVmBootNotificationServerAddressPrefix,
                      sizeof(kArcVmBootNotificationServerAddressPrefix) - 1);
  return address.append("-" +
                        base::Uuid::GenerateRandomV4().AsLowercaseString());
}

bool HasDiskImage(const vm_tools::concierge::StartArcVmRequest& request,
                  const std::string& disk_path) {
  for (const auto& disk : request.disks()) {
    if (disk.path() == disk_path) {
      return true;
    }
  }
  return false;
}

// A debugd client that can fail to start Concierge.
// TODO(khmel): Merge the feature to FakeDebugDaemonClient.
class TestDebugDaemonClient : public ash::FakeDebugDaemonClient {
 public:
  TestDebugDaemonClient() = default;

  TestDebugDaemonClient(const TestDebugDaemonClient&) = delete;
  TestDebugDaemonClient& operator=(const TestDebugDaemonClient&) = delete;

  ~TestDebugDaemonClient() override = default;

  void BackupArcBugReport(const cryptohome::AccountIdentifier& id,
                          chromeos::VoidDBusMethodCallback callback) override {
    backup_arc_bug_report_called_ = true;
    std::move(callback).Run(backup_arc_bug_report_result_);
  }

  bool backup_arc_bug_report_called() const {
    return backup_arc_bug_report_called_;
  }
  void set_backup_arc_bug_report_result(bool result) {
    backup_arc_bug_report_result_ = result;
  }

 private:
  bool backup_arc_bug_report_called_ = false;
  bool backup_arc_bug_report_result_ = true;
};

// A concierge that remembers the parameter passed to StartArcVm.
// TODO(khmel): Merge the feature to FakeConciergeClient.
class TestConciergeClient : public ash::FakeConciergeClient {
 public:
  static void Initialize() { new TestConciergeClient(); }

  TestConciergeClient(const TestConciergeClient&) = delete;
  TestConciergeClient& operator=(const TestConciergeClient&) = delete;

  ~TestConciergeClient() override = default;

  void StopVm(const vm_tools::concierge::StopVmRequest& request,
              chromeos::DBusMethodCallback<vm_tools::concierge::StopVmResponse>
                  callback) override {
    ++stop_vm_call_count_;
    stop_vm_request_ = request;
    ash::FakeConciergeClient::StopVm(request, std::move(callback));
    if (on_stop_vm_callback_ && (stop_vm_call_count_ == callback_count_)) {
      std::move(on_stop_vm_callback_).Run();
    }
  }

  void StartArcVm(
      const vm_tools::concierge::StartArcVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::StartVmResponse>
          callback) override {
    start_arc_vm_request_ = request;
    ash::FakeConciergeClient::StartArcVm(request, std::move(callback));
  }

  void ReclaimVmMemory(
      const vm_tools::concierge::ReclaimVmMemoryRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::ReclaimVmMemoryResponse>
          callback) override {
    ++reclaim_vm_count_;
    reclaim_vm_request_ = request;
    ash::FakeConciergeClient::ReclaimVmMemory(request, std::move(callback));
  }

  int stop_vm_call_count() const { return stop_vm_call_count_; }

  const vm_tools::concierge::StartArcVmRequest& start_arc_vm_request() const {
    return start_arc_vm_request_;
  }

  const vm_tools::concierge::StopVmRequest& stop_vm_request() const {
    return stop_vm_request_;
  }

  const vm_tools::concierge::ReclaimVmMemoryRequest& reclaim_vm_request()
      const {
    return reclaim_vm_request_;
  }

  int reclaim_vm_call_count() const { return reclaim_vm_count_; }

  // Set a callback to be run when stop_vm_call_count() == count.
  void set_on_stop_vm_callback(base::OnceClosure callback, int count) {
    on_stop_vm_callback_ = std::move(callback);
    DCHECK_NE(0, count);
    callback_count_ = count;
  }

 private:
  TestConciergeClient()
      : ash::FakeConciergeClient(/*fake_cicerone_client=*/nullptr) {}

  int stop_vm_call_count_ = 0;
  // When callback_count_ == 0, the on_stop_vm_callback_ is not run.
  int callback_count_ = 0;
  vm_tools::concierge::StartArcVmRequest start_arc_vm_request_;
  vm_tools::concierge::StopVmRequest stop_vm_request_;
  vm_tools::concierge::ReclaimVmMemoryRequest reclaim_vm_request_;
  int reclaim_vm_count_ = 0;
  base::OnceClosure on_stop_vm_callback_;
};

// A fake ArcVmBootNotificationServer that listens on an UDS and records
// connections and the data sent to it.
class TestArcVmBootNotificationServer
    : public base::MessagePumpForUI::FdWatcher {
 public:
  TestArcVmBootNotificationServer() = default;
  ~TestArcVmBootNotificationServer() override { Stop(); }
  TestArcVmBootNotificationServer(const TestArcVmBootNotificationServer&) =
      delete;
  TestArcVmBootNotificationServer& operator=(
      const TestArcVmBootNotificationServer&) = delete;

  // Creates a socket and binds it to a name in the abstract namespace, then
  // starts listening to the socket on another thread.
  void Start(const std::string& abstract_addr) {
    fd_.reset(socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0));
    ASSERT_TRUE(fd_.is_valid())
        << "open failed with " << base::safe_strerror(errno);

    sockaddr_un addr{.sun_family = AF_UNIX};
    ASSERT_LT(abstract_addr.size(), sizeof(addr.sun_path))
        << "abstract_addr is too long: " << abstract_addr;
    ASSERT_EQ('\0', abstract_addr[0])
        << "abstract_addr is not abstract: " << abstract_addr;
    memset(addr.sun_path, 0, sizeof(addr.sun_path));
    memcpy(addr.sun_path, abstract_addr.data(), abstract_addr.size());
    LOG(INFO) << "Abstract address: \\0" << &(addr.sun_path[1]);

    ASSERT_EQ(HANDLE_EINTR(bind(fd_.get(), reinterpret_cast<sockaddr*>(&addr),
                                sizeof(sockaddr_un))),
              0)
        << "bind failed with " << base::safe_strerror(errno);
    ASSERT_EQ(HANDLE_EINTR(listen(fd_.get(), 5)), 0)
        << "listen failed with " << base::safe_strerror(errno);
    controller_ =
        std::make_unique<base::MessagePumpForUI::FdWatchController>(FROM_HERE);
    ASSERT_TRUE(base::CurrentUIThread::Get()->WatchFileDescriptor(
        fd_.get(), true, base::MessagePumpForUI::WATCH_READ, controller_.get(),
        this));
  }

  // Release the socket.
  void Stop() {
    controller_.reset(nullptr);
    fd_.reset(-1);
  }

  int connection_count() { return num_connections_; }

  std::string received_data() { return received_; }

  // base::MessagePumpForUI::FdWatcher overrides
  void OnFileCanReadWithoutBlocking(int fd) override {
    base::ScopedFD client_fd(HANDLE_EINTR(accept(fd_.get(), nullptr, nullptr)));
    ASSERT_TRUE(client_fd.is_valid());

    ++num_connections_;

    // Attempt to read from connection until EOF
    std::string out;
    char buf[256];
    while (true) {
      ssize_t len = HANDLE_EINTR(read(client_fd.get(), buf, sizeof(buf)));
      if (len <= 0) {
        break;
      }
      out.append(buf, len);
    }
    received_.append(out);
  }

  void OnFileCanWriteWithoutBlocking(int fd) override {}

 private:
  base::ScopedFD fd_;
  std::unique_ptr<base::MessagePumpForUI::FdWatchController> controller_;
  int num_connections_ = 0;
  std::string received_;
};

class FakeDemoModeDelegate : public ArcClientAdapter::DemoModeDelegate {
 public:
  FakeDemoModeDelegate() = default;
  ~FakeDemoModeDelegate() override = default;
  FakeDemoModeDelegate(const FakeDemoModeDelegate&) = delete;
  FakeDemoModeDelegate& operator=(const FakeDemoModeDelegate&) = delete;

  void EnsureResourcesLoaded(base::OnceClosure callback) override {
    std::move(callback).Run();
  }

  base::FilePath GetDemoAppsPath() override { return base::FilePath(); }
};

class ArcVmClientAdapterTest : public testing::Test,
                               public ArcClientAdapter::Observer {
 public:
  ArcVmClientAdapterTest() {
    // Use the same VLOG() level as production. Note that
    // arc_vm_client_adapter.cc defines ENABLED_VLOG_LEVEL 1, which is respected
    // at compile time.
    logging::SetMinLogLevel(-1);

    // Create and set new fake clients every time to reset clients' status.
    test_debug_daemon_client_ = std::make_unique<TestDebugDaemonClient>();
    ash::DebugDaemonClient::SetInstanceForTest(test_debug_daemon_client_.get());
    TestConciergeClient::Initialize();
    ash::UpstartClient::InitializeFake();
  }

  ArcVmClientAdapterTest(const ArcVmClientAdapterTest&) = delete;
  ArcVmClientAdapterTest& operator=(const ArcVmClientAdapterTest&) = delete;

  ~ArcVmClientAdapterTest() override {
    ash::UpstartClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::DebugDaemonClient::SetInstanceForTest(nullptr);
    test_debug_daemon_client_.reset();
  }

  void SetUp() override {
    run_loop_ = std::make_unique<base::RunLoop>();
    adapter_ = CreateArcVmClientAdapterForTesting(base::BindRepeating(
        &ArcVmClientAdapterTest::RewriteStatus, base::Unretained(this)));
    adapter_->AddObserver(this);
    ASSERT_TRUE(dir_.CreateUniqueTempDir());

    host_rootfs_writable_ = false;
    system_image_ext_format_ = false;

    // The fake client returns VM_STATUS_STARTING by default. Change it
    // to VM_STATUS_RUNNING which is used by ARCVM.
    vm_tools::concierge::StartVmResponse start_vm_response;
    start_vm_response.set_status(vm_tools::concierge::VM_STATUS_RUNNING);
    auto* vm_info = start_vm_response.mutable_vm_info();
    vm_info->set_cid(kCid);
    GetTestConciergeClient()->set_start_vm_response(start_vm_response);

    // Reset to the original behavior.
    SetArcVmBootNotificationServerFdForTesting(std::nullopt);

    const std::string abstract_addr(GenerateAbstractAddress());
    boot_server_ = std::make_unique<TestArcVmBootNotificationServer>();
    boot_server_->Start(abstract_addr);
    SetArcVmBootNotificationServerAddressForTesting(
        abstract_addr,
        // connect_timeout_limit
        base::Milliseconds(100),
        // connect_sleep_duration_initial
        base::Milliseconds(20));

    ash::PatchPanelClient::InitializeFake();
    ash::SessionManagerClient::InitializeFake();

    adapter_->SetDemoModeDelegate(&demo_mode_delegate_);
    app_host_ = std::make_unique<FakeAppHost>(arc_bridge_service()->app());
    app_instance_ = std::make_unique<FakeAppInstance>(app_host_.get());
    arc_dlc_installer_ = std::make_unique<ArcDlcInstaller>();

    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
  }

  void TearDown() override {
    scoped_user_manager_.reset();
    arc_dlc_installer_.reset();
    ash::PatchPanelClient::Shutdown();
    ash::SessionManagerClient::Shutdown();
    adapter_->RemoveObserver(this);
    adapter_.reset();
    run_loop_.reset();
  }

  // ArcClientAdapter::Observer:
  void ArcInstanceStopped(bool is_system_shutdown) override {
    is_system_shutdown_ = is_system_shutdown;
    run_loop()->Quit();
  }

  void ExpectTrueThenQuit(bool result) {
    EXPECT_TRUE(result);
    run_loop()->Quit();
  }

  void ExpectFalseThenQuit(bool result) {
    EXPECT_FALSE(result);
    run_loop()->Quit();
  }

  void ExpectTrue(bool result) { EXPECT_TRUE(result); }

  void ExpectFalse(bool result) { EXPECT_FALSE(result); }

 protected:
  void SetAccountId(const AccountId& account_id) {
    arc_service_manager_.set_account_id(account_id);
  }

  void SetValidUserInfo() { SetUserInfo(kUserIdHash, kSerialNumber); }

  void SetUserInfo(const std::string& hash, const std::string& serial) {
    adapter()->SetUserInfo(
        cryptohome::Identification(user_manager::StubAccountId()), hash,
        serial);
  }

  void StartMiniArcWithParams(bool expect_success, StartParams params) {
    StartMiniArcWithParamsAndUser(expect_success, std::move(params),
                                  kUserIdHash, kSerialNumber);
  }

  void StartMiniArcWithParamsAndUser(bool expect_success,
                                     StartParams params,
                                     const std::string& hash,
                                     const std::string& serial) {
    SetUserInfo(hash, serial);
    adapter()->StartMiniArc(
        std::move(params),
        base::BindOnce(expect_success
                           ? &ArcVmClientAdapterTest::ExpectTrueThenQuit
                           : &ArcVmClientAdapterTest::ExpectFalseThenQuit,
                       base::Unretained(this)));

    run_loop()->Run();
    RecreateRunLoop();
  }

  void UpgradeArcWithParams(bool expect_success, UpgradeParams params) {
    adapter()->UpgradeArc(
        std::move(params),
        base::BindOnce(expect_success
                           ? &ArcVmClientAdapterTest::ExpectTrueThenQuit
                           : &ArcVmClientAdapterTest::ExpectFalseThenQuit,
                       base::Unretained(this)));
    run_loop()->Run();
    RecreateRunLoop();
  }

  void UpgradeArcWithParamsAndStopVmCount(bool expect_success,
                                          UpgradeParams params,
                                          int run_until_stop_vm_count) {
    GetTestConciergeClient()->set_on_stop_vm_callback(run_loop()->QuitClosure(),
                                                      run_until_stop_vm_count);
    adapter()->UpgradeArc(
        std::move(params),
        base::BindOnce(expect_success ? &ArcVmClientAdapterTest::ExpectTrue
                                      : &ArcVmClientAdapterTest::ExpectFalse,
                       base::Unretained(this)));
    run_loop()->Run();
    RecreateRunLoop();
  }

  // Starts mini instance with the default StartParams.
  void StartMiniArc() { StartMiniArcWithParams(true, {}); }

  // Upgrades the instance with the default UpgradeParams.
  void UpgradeArc(bool expect_success) {
    UpgradeArcWithParams(expect_success, {});
  }

  void SendVmStartedSignal() {
    vm_tools::concierge::VmStartedSignal signal;
    signal.set_name(kArcVmName);
    for (auto& observer : GetTestConciergeClient()->vm_observer_list()) {
      observer.OnVmStarted(signal);
    }
  }

  void SendVmStartedSignalNotForArcVm() {
    vm_tools::concierge::VmStartedSignal signal;
    signal.set_name("penguin");
    for (auto& observer : GetTestConciergeClient()->vm_observer_list()) {
      observer.OnVmStarted(signal);
    }
  }

  void SendVmStoppedSignalForCid(vm_tools::concierge::VmStopReason reason,
                                 int64_t cid) {
    vm_tools::concierge::VmStoppedSignal signal;
    signal.set_name(kArcVmName);
    signal.set_cid(cid);
    signal.set_reason(reason);
    for (auto& observer : GetTestConciergeClient()->vm_observer_list()) {
      observer.OnVmStopped(signal);
    }
  }

  void SendVmStoppedSignal(vm_tools::concierge::VmStopReason reason) {
    SendVmStoppedSignalForCid(reason, kCid);
  }

  void SendVmStoppedSignalNotForArcVm(
      vm_tools::concierge::VmStopReason reason) {
    vm_tools::concierge::VmStoppedSignal signal;
    signal.set_name("penguin");
    signal.set_cid(kCid);
    signal.set_reason(reason);
    for (auto& observer : GetTestConciergeClient()->vm_observer_list()) {
      observer.OnVmStopped(signal);
    }
  }

  void SendNameOwnerChangedSignal() {
    for (auto& observer : GetTestConciergeClient()->observer_list()) {
      observer.ConciergeServiceStopped();
    }
  }

  void InjectUpstartStartJobFailure(const std::string& job_name_to_fail) {
    auto* upstart_client = ash::FakeUpstartClient::Get();
    upstart_client->set_start_job_cb(base::BindLambdaForTesting(
        [job_name_to_fail](const std::string& job_name,
                           const std::vector<std::string>& env) {
          // Return success unless |job_name| is |job_name_to_fail|.
          return ash::FakeUpstartClient::StartJobResult(job_name !=
                                                        job_name_to_fail);
        }));
  }

  void InjectUpstartStopJobFailure(const std::string& job_name_to_fail) {
    auto* upstart_client = ash::FakeUpstartClient::Get();
    upstart_client->set_stop_job_cb(base::BindLambdaForTesting(
        [job_name_to_fail](const std::string& job_name,
                           const std::vector<std::string>& env) {
          // Return success unless |job_name| is |job_name_to_fail|.
          return job_name != job_name_to_fail;
        }));
  }

  // We expect ConciergeClient::StopVm to have been called two times,
  // once to clear a stale VM in StartMiniArc(), and another on this
  // call to StopArcInstance().
  void StopArcInstance() {
    adapter()->StopArcInstance(/*on_shutdown=*/false,
                               /*should_backup_log=*/false);
    run_loop()->RunUntilIdle();
    EXPECT_EQ(2, GetTestConciergeClient()->stop_vm_call_count());
    EXPECT_FALSE(is_system_shutdown().has_value());

    RecreateRunLoop();
    SendVmStoppedSignal(vm_tools::concierge::STOP_VM_REQUESTED);
    run_loop()->Run();
    ASSERT_TRUE(is_system_shutdown().has_value());
    EXPECT_FALSE(is_system_shutdown().value());
  }

  // Checks that ArcVmClientAdapter has requested to stop the VM (after an
  // error in UpgradeArc).
  // We expect ConciergeClient::StopVm to have been called two times,
  // once to clear a stale VM in StartMiniArc(), and another after some
  // error condition.
  void ExpectArcStopped() {
    EXPECT_EQ(2, GetTestConciergeClient()->stop_vm_call_count());
    EXPECT_FALSE(is_system_shutdown().has_value());
    RecreateRunLoop();
    SendVmStoppedSignal(vm_tools::concierge::STOP_VM_REQUESTED);
    run_loop()->Run();
    ASSERT_TRUE(is_system_shutdown().has_value());
    EXPECT_FALSE(is_system_shutdown().value());
  }

  void RecreateRunLoop() { run_loop_ = std::make_unique<base::RunLoop>(); }

  base::RunLoop* run_loop() { return run_loop_.get(); }
  ArcClientAdapter* adapter() { return adapter_.get(); }

  const std::optional<bool>& is_system_shutdown() const {
    return is_system_shutdown_;
  }
  void reset_is_system_shutdown() { is_system_shutdown_ = std::nullopt; }
  TestConciergeClient* GetTestConciergeClient() {
    return static_cast<TestConciergeClient*>(ash::ConciergeClient::Get());
  }

  TestDebugDaemonClient* test_debug_daemon_client() {
    return test_debug_daemon_client_.get();
  }

  TestArcVmBootNotificationServer* boot_notification_server() {
    return boot_server_.get();
  }

  void set_block_apex_path(base::FilePath block_apex_path) {
    block_apex_path_ = block_apex_path;
  }

  void set_host_rootfs_writable(bool host_rootfs_writable) {
    host_rootfs_writable_ = host_rootfs_writable;
  }

  void set_system_image_ext_format(bool system_image_ext_format) {
    system_image_ext_format_ = system_image_ext_format;
  }

  ArcBridgeService* arc_bridge_service() {
    return arc_service_manager_.arc_bridge_service();
  }
  FakeAppInstance* app_instance() { return app_instance_.get(); }

 private:
  void RewriteStatus(FileSystemStatus* status) {
    status->set_block_apex_path_for_testing(block_apex_path_);
    status->set_host_rootfs_writable_for_testing(host_rootfs_writable_);
    status->set_system_image_ext_format_for_testing(system_image_ext_format_);
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<ArcClientAdapter> adapter_;
  std::optional<bool> is_system_shutdown_;

  content::BrowserTaskEnvironment browser_task_environment_;
  base::ScopedTempDir dir_;
  ArcServiceManager arc_service_manager_;

  // Variables to override the value in FileSystemStatus.
  base::FilePath block_apex_path_;
  bool host_rootfs_writable_;
  bool system_image_ext_format_;

  std::unique_ptr<TestArcVmBootNotificationServer> boot_server_;

  FakeDemoModeDelegate demo_mode_delegate_;
  std::unique_ptr<FakeAppHost> app_host_;
  std::unique_ptr<FakeAppInstance> app_instance_;
  std::unique_ptr<ArcDlcInstaller> arc_dlc_installer_;
  std::unique_ptr<TestDebugDaemonClient> test_debug_daemon_client_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

// Tests that SetUserInfo() doesn't crash.
TEST_F(ArcVmClientAdapterTest, SetUserInfo) {
  SetUserInfo(kUserIdHash, kSerialNumber);
}

// Tests that SetUserInfo() doesn't crash even when empty strings are passed.
// Currently, ArcSessionRunner's tests call SetUserInfo() that way.
// TODO(khmel): Once ASR's tests are fixed, remove this test and use DCHECKs
// in SetUserInfo().
TEST_F(ArcVmClientAdapterTest, SetUserInfoEmpty) {
  adapter()->SetUserInfo(cryptohome::Identification(), std::string(),
                         std::string());
}

// Tests that StartMiniArc() succeeds by default.
TEST_F(ArcVmClientAdapterTest, StartMiniArc) {
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  // No GetVmInfo call is expected
  EXPECT_EQ(0, GetTestConciergeClient()->get_vm_info_call_count());
  // Expect StopVm() to be called  once in StartMiniArc to clear stale
  // VM.
  EXPECT_EQ(1, GetTestConciergeClient()->stop_vm_call_count());

  StopArcInstance();
}

TEST_F(ArcVmClientAdapterTest, StartMiniArcEmptyUserIdHash) {
  StartMiniArcWithParamsAndUser(false, {}, std::string(), kSerialNumber);

  EXPECT_EQ(0, GetTestConciergeClient()->start_arc_vm_call_count());
  // No GetVmInfo call is expected
  EXPECT_EQ(0, GetTestConciergeClient()->get_vm_info_call_count());
  // Expect StopVm() to be called  once in StartMiniArc to clear stale
  // VM.
  EXPECT_EQ(1, GetTestConciergeClient()->stop_vm_call_count());
}

// Tests that StartMiniArc() still succeeds without the feature.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_WithPerVCpuCoreScheduling) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(kEnablePerVmCoreScheduling,
                                    false /* use */);

  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);

  StopArcInstance();
}

// Tests that StartMiniArc() still succeeds even when Upstart fails to stop
// the arcvm-post-login-services job.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_StopArcVmPostLoginServicesJobFail) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStopJobFailure(kArcVmPostLoginServicesJobName);

  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);

  StopArcInstance();
}

// Tests that StartMiniArc() still succeeds even when Upstart fails to stop
// arcvm-media-sharing-services.
TEST_F(ArcVmClientAdapterTest,
       StartMiniArc_StopArcVmMediaSharingServicesJobFail) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStopJobFailure(kArcVmMediaSharingServicesJobName);

  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);

  StopArcInstance();
}

// Tests that StartMiniArc() still succeeds even when Upstart fails to stop
// arcvm-data-migrator.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_StopArcVmDataMigratorJobFail) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStopJobFailure(kArcVmDataMigratorJobName);

  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);

  StopArcInstance();
}

// Tests that StartMiniArc() fails when Upstart fails to start the job.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_StartArcVmPerBoardFeaturesJobFail) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStartJobFailure(kArcVmPerBoardFeaturesJobName);

  StartMiniArcWithParams(false, {});

  // Confirm that no VM is started.
  EXPECT_EQ(GetTestConciergeClient()->start_arc_vm_call_count(), 0);
}

// Tests that StartMiniArc() fails if Upstart fails to start
// arcvm-pre-login-services.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_StartArcVmPreLoginServicesJobFail) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStartJobFailure(kArcVmPreLoginServicesJobName);

  StartMiniArcWithParams(false, {});
  EXPECT_EQ(GetTestConciergeClient()->start_arc_vm_call_count(), 0);
}

// Tests that StartMiniArc() succeeds if Upstart fails to stop
// arcvm-pre-login-services.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_StopArcVmPreLoginServicesJobFail) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStopJobFailure(kArcVmPreLoginServicesJobName);

  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);

  StopArcInstance();
}

// Tests that |kArcVmPreLoginServicesJobName| is properly stopped and then
// started in StartMiniArc().
TEST_F(ArcVmClientAdapterTest, StartMiniArc_JobRestart) {
  ash::FakeUpstartClient::Get()->StartRecordingUpstartOperations();
  StartMiniArc();

  const auto& ops =
      ash::FakeUpstartClient::Get()->GetRecordedUpstartOperationsForJob(
          kArcVmPreLoginServicesJobName);
  ASSERT_EQ(ops.size(), 2u);
  EXPECT_EQ(ops[0].type, ash::FakeUpstartClient::UpstartOperationType::STOP);
  EXPECT_EQ(ops[1].type, ash::FakeUpstartClient::UpstartOperationType::START);
}

// Tests that StopArcInstance() eventually notifies the observer.
TEST_F(ArcVmClientAdapterTest, StopArcInstance) {
  StartMiniArc();
  UpgradeArc(true);

  adapter()->StopArcInstance(/*on_shutdown=*/false,
                             /*should_backup_log=*/false);
  run_loop()->RunUntilIdle();
  EXPECT_EQ(2, GetTestConciergeClient()->stop_vm_call_count());
  // The callback for StopVm D-Bus reply does NOT call ArcInstanceStopped when
  // the D-Bus call result is successful.
  EXPECT_FALSE(is_system_shutdown().has_value());

  // Instead, vm_concierge explicitly notifies Chrome of the VM termination.
  RecreateRunLoop();
  SendVmStoppedSignal(vm_tools::concierge::STOP_VM_REQUESTED);
  run_loop()->Run();
  // ..and that calls ArcInstanceStopped.
  ASSERT_TRUE(is_system_shutdown().has_value());
  EXPECT_FALSE(is_system_shutdown().value());
}

// b/164816080 This test ensures that a new vm instance that is
// created while handling the shutting down of the previous instance,
// doesn't incorrectly receive the shutdown event as well.
TEST_F(ArcVmClientAdapterTest, DoesNotGetArcInstanceStoppedOnNestedInstance) {
  using RunLoopFactory = base::RepeatingCallback<base::RunLoop*()>;

  class Observer : public ArcClientAdapter::Observer {
   public:
    Observer(RunLoopFactory run_loop_factory, Observer* child_observer)
        : run_loop_factory_(run_loop_factory),
          child_observer_(child_observer) {}
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    ~Observer() override {
      if (child_observer_ && nested_adapter_) {
        nested_adapter_->RemoveObserver(child_observer_);
      }
    }

    bool stopped_called() const { return stopped_called_; }

    // ArcClientAdapter::Observer:
    void ArcInstanceStopped(bool is_system_shutdown) override {
      stopped_called_ = true;

      if (child_observer_) {
        nested_adapter_ = CreateArcVmClientAdapterForTesting(base::DoNothing());
        nested_adapter_->AddObserver(child_observer_);
        nested_adapter_->SetUserInfo(
            cryptohome::Identification(user_manager::StubAccountId()),
            kUserIdHash, kSerialNumber);
        nested_adapter_->SetDemoModeDelegate(&demo_mode_delegate_);

        base::RunLoop* run_loop = run_loop_factory_.Run();
        nested_adapter_->StartMiniArc({}, QuitClosure(run_loop));
        run_loop->Run();

        run_loop = run_loop_factory_.Run();
        nested_adapter_->UpgradeArc({}, QuitClosure(run_loop));
        run_loop->Run();
      }
    }

   private:
    base::OnceCallback<void(bool)> QuitClosure(base::RunLoop* run_loop) {
      return base::BindOnce(
          [](base::RunLoop* run_loop, bool result) { run_loop->Quit(); },
          run_loop);
    }

    base::RepeatingCallback<base::RunLoop*()> const run_loop_factory_;
    const raw_ptr<Observer> child_observer_;
    std::unique_ptr<ArcClientAdapter> nested_adapter_;
    FakeDemoModeDelegate demo_mode_delegate_;
    bool stopped_called_ = false;
  };

  StartMiniArc();
  UpgradeArc(true);

  RunLoopFactory run_loop_factory = base::BindLambdaForTesting([this]() {
    RecreateRunLoop();
    return run_loop();
  });

  Observer child_observer(run_loop_factory, nullptr);
  Observer parent_observer(run_loop_factory, &child_observer);
  adapter()->AddObserver(&parent_observer);
  absl::Cleanup teardown = [this, &parent_observer] {
    adapter()->RemoveObserver(&parent_observer);
  };

  SendVmStoppedSignal(vm_tools::concierge::STOP_VM_REQUESTED);

  EXPECT_TRUE(parent_observer.stopped_called());
  EXPECT_FALSE(child_observer.stopped_called());
}

// Tests that StopArcInstance() initiates ARC log backup.
TEST_F(ArcVmClientAdapterTest, StopArcInstance_WithLogBackup) {
  StartMiniArc();
  UpgradeArc(true);

  adapter()->StopArcInstance(/*on_shutdown=*/false, /*should_backup_log=*/true);
  run_loop()->RunUntilIdle();
  EXPECT_EQ(2, GetTestConciergeClient()->stop_vm_call_count());
  // The callback for StopVm D-Bus reply does NOT call ArcInstanceStopped when
  // the D-Bus call result is successful.
  EXPECT_FALSE(is_system_shutdown().has_value());

  // Instead, vm_concierge explicitly notifies Chrome of the VM termination.
  RecreateRunLoop();
  SendVmStoppedSignal(vm_tools::concierge::STOP_VM_REQUESTED);
  run_loop()->Run();
  // ..and that calls ArcInstanceStopped.
  ASSERT_TRUE(is_system_shutdown().has_value());
  EXPECT_FALSE(is_system_shutdown().value());
}

TEST_F(ArcVmClientAdapterTest, StopArcInstance_WithLogBackup_BackupFailed) {
  StartMiniArc();
  UpgradeArc(true);

  EXPECT_FALSE(test_debug_daemon_client()->backup_arc_bug_report_called());
  test_debug_daemon_client()->set_backup_arc_bug_report_result(false);

  adapter()->StopArcInstance(/*on_shutdown=*/false, /*should_backup_log=*/true);
  run_loop()->RunUntilIdle();
  EXPECT_EQ(2, GetTestConciergeClient()->stop_vm_call_count());
  // The callback for StopVm D-Bus reply does NOT call ArcInstanceStopped when
  // the D-Bus call result is successful.
  EXPECT_FALSE(is_system_shutdown().has_value());

  // Instead, vm_concierge explicitly notifies Chrome of the VM termination.
  RecreateRunLoop();
  SendVmStoppedSignal(vm_tools::concierge::STOP_VM_REQUESTED);
  run_loop()->Run();

  EXPECT_TRUE(test_debug_daemon_client()->backup_arc_bug_report_called());
  // ..and that calls ArcInstanceStopped.
  ASSERT_TRUE(is_system_shutdown().has_value());
  EXPECT_FALSE(is_system_shutdown().value());
}

// Tests that StopArcInstance() called during shutdown doesn't do anything.
TEST_F(ArcVmClientAdapterTest, StopArcInstance_OnShutdown) {
  StartMiniArc();
  UpgradeArc(true);

  adapter()->StopArcInstance(/*on_shutdown=*/true, /*should_backup_log=*/false);
  run_loop()->RunUntilIdle();
  EXPECT_EQ(1, GetTestConciergeClient()->stop_vm_call_count());
  EXPECT_FALSE(is_system_shutdown().has_value());
}

// Tests that StopArcInstance() immediately notifies the observer on failure.
TEST_F(ArcVmClientAdapterTest, StopArcInstance_Fail) {
  StartMiniArc();
  UpgradeArc(true);

  // Inject failure.
  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  GetTestConciergeClient()->set_stop_vm_response(response);

  adapter()->StopArcInstance(/*on_shutdown=*/false,
                             /*should_backup_log=*/false);

  run_loop()->Run();
  EXPECT_EQ(2, GetTestConciergeClient()->stop_vm_call_count());

  // The callback for StopVm D-Bus reply does call ArcInstanceStopped when
  // the D-Bus call result is NOT successful.
  ASSERT_TRUE(is_system_shutdown().has_value());
  EXPECT_FALSE(is_system_shutdown().value());
}

// Test that StopArcInstance() stops the VM if only mini-ARCVM
// is called.
TEST_F(ArcVmClientAdapterTest, StopArcInstance_StopMiniVm) {
  StartMiniArc();

  adapter()->StopArcInstance(/*on_shutdown=*/false,
                             /*should_backup_log*/ false);
  run_loop()->RunUntilIdle();

  // No GetVmInfo call is expected.
  EXPECT_EQ(0, GetTestConciergeClient()->get_vm_info_call_count());
  // Expect StopVm() to be called twice; once in StartMiniArc to clear stale
  // VM, and again on StopArcInstance().
  EXPECT_EQ(2, GetTestConciergeClient()->stop_vm_call_count());
  EXPECT_EQ(kUserIdHash,
            GetTestConciergeClient()->stop_vm_request().owner_id());
}

// Tests that UpgradeArc() handles arcvm-post-login-services startup failures
// properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_StartArcVmPostLoginServicesFailure) {
  StartMiniArc();

  // Inject failure to FakeUpstartClient.
  InjectUpstartStartJobFailure(kArcVmPostLoginServicesJobName);

  UpgradeArcWithParamsAndStopVmCount(false, {}, /*run_until_stop_vm_count=*/2);

  ExpectArcStopped();
}

// Tests that StartMiniArc()'s JOB_STOP_AND_START for
// |kArcVmPreLoginServicesJobName| does not have DISABLE_UREADAHEAD variable
// by default.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_UreadaheadByDefault) {
  StartParams start_params(GetPopulatedStartParams());
  ash::FakeUpstartClient::Get()->StartRecordingUpstartOperations();
  StartMiniArcWithParams(true, std::move(start_params));

  const auto& ops =
      ash::FakeUpstartClient::Get()->GetRecordedUpstartOperationsForJob(
          kArcVmPreLoginServicesJobName);
  ASSERT_EQ(ops.size(), 2u);
  EXPECT_EQ(ops[0].type, ash::FakeUpstartClient::UpstartOperationType::STOP);
  EXPECT_EQ(ops[1].type, ash::FakeUpstartClient::UpstartOperationType::START);
  EXPECT_TRUE(ops[1].env.empty());
}

// Tests that StartMiniArc() handles arcvm-post-vm-start-services stop failures
// properly.
TEST_F(ArcVmClientAdapterTest,
       StartMiniArc_StopArcVmPostVmStartServicesFailure) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStopJobFailure(kArcVmPostVmStartServicesJobName);

  // StartMiniArc should still succeed.
  StartMiniArc();

  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());

  // Make sure StopVm() is called only once, to stop existing VMs on
  // StartMiniArc().
  EXPECT_EQ(1, GetTestConciergeClient()->stop_vm_call_count());
}

// Tests that UpgradeArc() handles arcvm-post-vm-start-services startup
// failures properly.
TEST_F(ArcVmClientAdapterTest,
       UpgradeArc_StartArcVmPostVmStartServicesFailure) {
  StartMiniArc();

  // Inject failure to FakeUpstartClient.
  InjectUpstartStartJobFailure(kArcVmPostVmStartServicesJobName);
  // UpgradeArc should fail and the VM should be stoppped.
  UpgradeArcWithParamsAndStopVmCount(false, {}, /*run_until_stop_vm_count=*/2);
  ExpectArcStopped();
}

// Tests that a "Failed Adb Sideload response" case is handled properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_FailedAdbResponse) {
  StartMiniArc();

  // Ask the Fake Session Manager to return a failed Adb Sideload response.
  ash::FakeSessionManagerClient::Get()->set_adb_sideload_response(
      ash::FakeSessionManagerClient::AdbSideloadResponseCode::FAILED);

  UpgradeArcWithParamsAndStopVmCount(false, {}, /*run_until_stop_vm_count=*/2);
  ExpectArcStopped();
}

// Tests that a "Need_Powerwash Adb Sideload response" case is handled properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_NeedPowerwashAdbResponse) {
  StartMiniArc();

  // Ask the Fake Session Manager to return a Need_Powerwash Adb Sideload
  // response.
  ash::FakeSessionManagerClient::Get()->set_adb_sideload_response(
      ash::FakeSessionManagerClient::AdbSideloadResponseCode::NEED_POWERWASH);
  UpgradeArc(true);
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  EXPECT_TRUE(base::Contains(boot_notification_server()->received_data(),
                             "ro.boot.enable_adb_sideloading=0"));
}

// Tests that adb sideloading is disabled by default.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_AdbSideloadingPropertyDefault) {
  StartMiniArc();

  UpgradeArc(true);
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  EXPECT_TRUE(base::Contains(boot_notification_server()->received_data(),
                             "ro.boot.enable_adb_sideloading=0"));
}

// Tests that adb sideloading can be controlled via session_manager.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_AdbSideloadingPropertyEnabled) {
  StartMiniArc();

  ash::FakeSessionManagerClient::Get()->set_adb_sideload_enabled(true);
  UpgradeArc(true);
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  EXPECT_TRUE(base::Contains(boot_notification_server()->received_data(),
                             "ro.boot.enable_adb_sideloading=1"));
}

TEST_F(ArcVmClientAdapterTest, UpgradeArc_AdbSideloadingPropertyDisabled) {
  StartMiniArc();

  ash::FakeSessionManagerClient::Get()->set_adb_sideload_enabled(false);
  UpgradeArc(true);
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  EXPECT_TRUE(base::Contains(boot_notification_server()->received_data(),
                             "ro.boot.enable_adb_sideloading=0"));
}

// Tests that "no serial" failure is handled properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_NoSerial) {
  // Don't set the serial number.
  StartMiniArcWithParamsAndUser(true, {}, kUserIdHash, std::string());
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);

  UpgradeArcWithParamsAndStopVmCount(false, {}, /*run_until_stop_vm_count=*/2);
  ExpectArcStopped();
}

TEST_F(ArcVmClientAdapterTest, StartMiniArc_StopExistingVmFailure) {
  // Inject failure.
  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  GetTestConciergeClient()->set_stop_vm_response(response);

  StartMiniArcWithParams(false, {});

  EXPECT_EQ(GetTestConciergeClient()->start_arc_vm_call_count(), 0);
  EXPECT_FALSE(is_system_shutdown().has_value());
}

TEST_F(ArcVmClientAdapterTest, StartMiniArc_StopExistingVmFailureEmptyReply) {
  // Inject failure.
  GetTestConciergeClient()->set_stop_vm_response(std::nullopt);

  StartMiniArcWithParams(false, {});

  EXPECT_EQ(GetTestConciergeClient()->start_arc_vm_call_count(), 0);
  EXPECT_FALSE(is_system_shutdown().has_value());
}

// Tests that ConciergeClient::WaitForServiceToBeAvailable() failure is handled
// properly.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_WaitForConciergeAvailableFailure) {
  // Inject failure.
  GetTestConciergeClient()->set_wait_for_service_to_be_available_response(
      false);

  StartMiniArcWithParams(false, {});
  EXPECT_EQ(GetTestConciergeClient()->start_arc_vm_call_count(), 0);
  EXPECT_FALSE(is_system_shutdown().has_value());
}

// Tests that StartArcVm() failure is handled properly.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_StartArcVmFailure) {
  // Inject failure to StartArcVm().
  vm_tools::concierge::StartVmResponse start_vm_response;
  start_vm_response.set_status(vm_tools::concierge::VM_STATUS_UNKNOWN);
  GetTestConciergeClient()->set_start_vm_response(start_vm_response);

  StartMiniArcWithParams(false, {});

  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
}

TEST_F(ArcVmClientAdapterTest, StartMiniArc_StartArcVmFailureEmptyReply) {
  // Inject failure to StartArcVm(). This emulates D-Bus timeout situations.
  GetTestConciergeClient()->set_start_vm_response(std::nullopt);

  StartMiniArcWithParams(false, {});

  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
}

// Tests that successful StartArcVm() call is handled properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_Success) {
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  UpgradeArc(true);

  StopArcInstance();
}

// Try to start and upgrade the instance with more params.
TEST_F(ArcVmClientAdapterTest, StartUpgradeArc_VariousParams) {
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));

  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());

  UpgradeParams params(GetPopulatedUpgradeParams());
  UpgradeArcWithParams(true, std::move(params));
}

// Try to start and upgrade the instance with slightly different params
// than StartUpgradeArc_VariousParams for better code coverage.
TEST_F(ArcVmClientAdapterTest, StartUpgradeArc_VariousParams2) {
  StartParams start_params(GetPopulatedStartParams());
  // Use slightly different params than StartUpgradeArc_VariousParams.
  start_params.play_store_auto_update =
      StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_OFF;

  StartMiniArcWithParams(true, std::move(start_params));

  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());

  UpgradeParams params(GetPopulatedUpgradeParams());
  // Use slightly different params than StartUpgradeArc_VariousParams.
  params.packages_cache_mode =
      UpgradeParams::PackageCacheMode::SKIP_SETUP_COPY_ON_INIT;
  params.management_transition = ArcManagementTransition::REGULAR_TO_CHILD;
  params.preferred_languages = {"en_US"};

  UpgradeArcWithParams(true, std::move(params));
}

// Try to start and upgrade the instance with demo mode enabled.
TEST_F(ArcVmClientAdapterTest, StartUpgradeArc_DemoMode) {
  constexpr char kDemoImage[] =
      "/run/imageloader/demo-mode-resources/0.0.1.7/android_demo_apps.squash";
  base::FilePath apps_path = base::FilePath(kDemoImage);

  class TestDemoDelegate : public ArcClientAdapter::DemoModeDelegate {
   public:
    explicit TestDemoDelegate(base::FilePath apps_path)
        : apps_path_(apps_path) {}
    ~TestDemoDelegate() override = default;

    void EnsureResourcesLoaded(base::OnceClosure callback) override {
      std::move(callback).Run();
    }

    base::FilePath GetDemoAppsPath() override { return apps_path_; }

   private:
    base::FilePath apps_path_;
  };

  TestDemoDelegate delegate(apps_path);
  adapter()->SetDemoModeDelegate(&delegate);
  StartMiniArc();

  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());

  // Verify the request.
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  // Make sure disks have the squashfs image.
  EXPECT_TRUE(HasDiskImage(request, kDemoImage));

  UpgradeParams params(GetPopulatedUpgradeParams());
  // Enable demo mode.
  params.is_demo_session = true;

  UpgradeArcWithParams(true, std::move(params));
  EXPECT_TRUE(base::Contains(boot_notification_server()->received_data(),
                             "ro.boot.arc_demo_mode=1"));
}

TEST_F(ArcVmClientAdapterTest, StartUpgradeArc_DisableMediaStoreMaintenance) {
  StartParams start_params(GetPopulatedStartParams());
  start_params.disable_media_store_maintenance = true;
  StartMiniArcWithParams(true, std::move(start_params));
  UpgradeParams params(GetPopulatedUpgradeParams());
  UpgradeArcWithParams(true, std::move(params));
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(
      request.mini_instance_request().disable_media_store_maintenance());
}

TEST_F(ArcVmClientAdapterTest, StartUpgradeArc_ArcVmUreadaheadMode) {
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  UpgradeParams params(GetPopulatedUpgradeParams());
  UpgradeArcWithParams(true, std::move(params));
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(request.ureadahead_mode(),
            vm_tools::concierge::StartArcVmRequest::UREADAHEAD_MODE_READAHEAD);
}

TEST_F(ArcVmClientAdapterTest, StartMiniArc_EnablePaiGeneration) {
  StartParams start_params(GetPopulatedStartParams());
  start_params.arc_generate_play_auto_install = true;
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(request.mini_instance_request().arc_generate_pai());
}

TEST_F(ArcVmClientAdapterTest, StartMiniArc_PaiGenerationDefaultDisabled) {
  StartMiniArcWithParams(true, GetPopulatedStartParams());
  // No androidboot property should be generated.
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(request.mini_instance_request().arc_generate_pai());
}

// Tests that StartArcVm() is called with valid parameters.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_StartArcVmParams) {
  StartMiniArc();
  ASSERT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);

  // Verify parameters
  const auto& params = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ("arcvm", params.name());
  EXPECT_LT(0u, params.cpus());
  // Make sure vendor.raw.img is passed.
  EXPECT_LE(1, params.disks_size());
}

// Tests that crosvm crash is handled properly.
TEST_F(ArcVmClientAdapterTest, CrosvmCrash) {
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  UpgradeArc(true);

  // Kill crosvm and verify StopArcInstance is called.
  SendVmStoppedSignal(vm_tools::concierge::VM_EXITED);
  run_loop()->Run();
  ASSERT_TRUE(is_system_shutdown().has_value());
  EXPECT_FALSE(is_system_shutdown().value());
}

// Tests that vm_concierge shutdown is handled properly.
TEST_F(ArcVmClientAdapterTest, ConciergeShutdown) {
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  UpgradeArc(true);

  // vm_concierge sends a VmStoppedSignal when shutting down.
  SendVmStoppedSignal(vm_tools::concierge::SERVICE_SHUTDOWN);
  run_loop()->Run();
  ASSERT_TRUE(is_system_shutdown().has_value());
  EXPECT_TRUE(is_system_shutdown().value());

  // Verify StopArcInstance is NOT called when vm_concierge stops since
  // the observer has already been called.
  RecreateRunLoop();
  reset_is_system_shutdown();
  SendNameOwnerChangedSignal();
  run_loop()->RunUntilIdle();
  EXPECT_FALSE(is_system_shutdown().has_value());
}

// Tests that vm_concierge crash is handled properly.
TEST_F(ArcVmClientAdapterTest, ConciergeCrash) {
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  UpgradeArc(true);

  // Kill vm_concierge and verify StopArcInstance is called.
  SendNameOwnerChangedSignal();
  run_loop()->Run();
  ASSERT_TRUE(is_system_shutdown().has_value());
  EXPECT_FALSE(is_system_shutdown().value());
}

// Tests the case where crosvm crashes, then vm_concierge crashes too.
TEST_F(ArcVmClientAdapterTest, CrosvmAndConciergeCrashes) {
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  UpgradeArc(true);

  // Kill crosvm and verify StopArcInstance is called.
  SendVmStoppedSignal(vm_tools::concierge::VM_EXITED);
  run_loop()->Run();
  ASSERT_TRUE(is_system_shutdown().has_value());
  EXPECT_FALSE(is_system_shutdown().value());

  // Kill vm_concierge and verify StopArcInstance is NOT called since
  // the observer has already been called.
  RecreateRunLoop();
  reset_is_system_shutdown();
  SendNameOwnerChangedSignal();
  run_loop()->RunUntilIdle();
  EXPECT_FALSE(is_system_shutdown().has_value());
}

// Tests the case where a unknown VmStopped signal is sent to Chrome.
TEST_F(ArcVmClientAdapterTest, VmStoppedSignal_UnknownCid) {
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  UpgradeArc(true);

  SendVmStoppedSignalForCid(vm_tools::concierge::STOP_VM_REQUESTED,
                            42);  // unknown CID
  run_loop()->RunUntilIdle();
  EXPECT_FALSE(is_system_shutdown().has_value());
}

// Tests the case where a stale VmStopped signal is sent to Chrome.
TEST_F(ArcVmClientAdapterTest, VmStoppedSignal_Stale) {
  SendVmStoppedSignalForCid(vm_tools::concierge::STOP_VM_REQUESTED, 42);
  run_loop()->RunUntilIdle();
  EXPECT_FALSE(is_system_shutdown().has_value());
}

// Tests the case where a VmStopped signal not for ARCVM (e.g. Termina) is sent
// to Chrome.
TEST_F(ArcVmClientAdapterTest, VmStoppedSignal_Termina) {
  SendVmStoppedSignalNotForArcVm(vm_tools::concierge::STOP_VM_REQUESTED);
  run_loop()->RunUntilIdle();
  EXPECT_FALSE(is_system_shutdown().has_value());
}

// Tests that receiving VmStarted signal is no-op.
TEST_F(ArcVmClientAdapterTest, VmStartedSignal) {
  SendVmStartedSignal();
  run_loop()->RunUntilIdle();
  RecreateRunLoop();
  SendVmStartedSignalNotForArcVm();
  run_loop()->RunUntilIdle();
}

// Tests that ConciergeServiceStarted() doesn't crash.
TEST_F(ArcVmClientAdapterTest, TestConciergeServiceStarted) {
  GetTestConciergeClient()->NotifyConciergeStarted();
}

// Tests that the kernel parameter does not include "rw" by default.
TEST_F(ArcVmClientAdapterTest, KernelParam_RO) {
  set_host_rootfs_writable(false);
  set_system_image_ext_format(false);
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);

  // Check "rw" is not in |params|.
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(request.rootfs_writable());
}

// Tests that the kernel parameter does include "rw" when '/' is writable and
// the image is in ext4.
TEST_F(ArcVmClientAdapterTest, KernelParam_RW) {
  set_host_rootfs_writable(true);
  set_system_image_ext_format(true);
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);

  // Check "rw" is in |params|.
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(request.rootfs_writable());
}

// Tests that CreateArcVmClientAdapter() doesn't crash.
TEST_F(ArcVmClientAdapterTest, TestCreateArcVmClientAdapter) {
  CreateArcVmClientAdapter();
}

TEST_F(ArcVmClientAdapterTest, DefaultBlockSize) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(arc::kUseDefaultBlockSize, true /* use */);

  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  EXPECT_EQ(
      0u, GetTestConciergeClient()->start_arc_vm_request().rootfs_block_size());
}

TEST_F(ArcVmClientAdapterTest, SpecifyBlockSize) {
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  EXPECT_EQ(
      4096u,
      GetTestConciergeClient()->start_arc_vm_request().rootfs_block_size());
}

TEST_F(ArcVmClientAdapterTest, VirtioBlkMultipleWorkers_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(arc::kEnableVirtioBlkMultipleWorkers);

  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));

  // All disks should have multiple workers disabled.
  EXPECT_FALSE(GetTestConciergeClient()
                   ->start_arc_vm_request()
                   .rootfs_multiple_workers());
  for (const auto& disk :
       GetTestConciergeClient()->start_arc_vm_request().disks()) {
    EXPECT_FALSE(disk.multiple_workers());
  }
}

TEST_F(ArcVmClientAdapterTest, VirtioBlkMultipleWorkers_Enabled_NoBlkData) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(arc::kEnableVirtioBlkMultipleWorkers);

  StartParams start_params(GetPopulatedStartParams());
  start_params.use_virtio_blk_data = false;
  StartMiniArcWithParams(true, std::move(start_params));

  // rootfs should have multiple workers enabled.
  EXPECT_TRUE(GetTestConciergeClient()
                  ->start_arc_vm_request()
                  .rootfs_multiple_workers());
  // No other disks should have multiple workers enabled.
  for (const auto& disk :
       GetTestConciergeClient()->start_arc_vm_request().disks()) {
    EXPECT_FALSE(disk.multiple_workers());
  }
}

TEST_F(ArcVmClientAdapterTest, VirtioBlkMultipleWorkers_Enabled_BlkData) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(arc::kEnableVirtioBlkMultipleWorkers);

  GetTestConciergeClient()->set_create_disk_image_response(
      CreateDiskImageResponse(vm_tools::concierge::DISK_STATUS_CREATED));

  StartParams start_params(GetPopulatedStartParams());
  start_params.use_virtio_blk_data = true;
  StartMiniArcWithParams(true, std::move(start_params));

  const auto& req = GetTestConciergeClient()->start_arc_vm_request();

  // rootfs should have multiple workers enabled.
  EXPECT_TRUE(req.rootfs_multiple_workers());
  EXPECT_TRUE(HasDiskImage(req, kCreatedDiskImagePath));
  for (const auto& disk :
       GetTestConciergeClient()->start_arc_vm_request().disks()) {
    // The data disk should have multiple workers enabled.
    if (disk.path() == kCreatedDiskImagePath) {
      EXPECT_TRUE(disk.multiple_workers());
    } else {
      EXPECT_FALSE(disk.multiple_workers());
    }
  }
}

TEST_F(ArcVmClientAdapterTest, VirtioBlkForData_Disabled) {
  GetTestConciergeClient()->set_create_disk_image_response(
      CreateDiskImageResponse(vm_tools::concierge::DISK_STATUS_CREATED));

  StartParams start_params(GetPopulatedStartParams());
  start_params.use_virtio_blk_data = false;
  StartMiniArcWithParams(true, std::move(start_params));
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);

  // CreateDiskImage() should NOT be called.
  EXPECT_EQ(GetTestConciergeClient()->create_disk_image_call_count(), 0);

  // StartArcVmRequest should NOT contain a disk created by CreateDiskImage().
  const auto& req = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(HasDiskImage(req, kCreatedDiskImagePath));
  EXPECT_FALSE(req.enable_virtio_blk_data());
}

TEST_F(ArcVmClientAdapterTest, VirtioBlkForData_CreateDiskimageResponseEmpty) {
  // CreateDiskImage() returns an empty response.
  GetTestConciergeClient()->set_create_disk_image_response(std::nullopt);

  // StartArcVm should NOT be called.
  StartParams start_params(GetPopulatedStartParams());
  start_params.use_virtio_blk_data = true;
  StartMiniArcWithParams(false, std::move(start_params));
  EXPECT_EQ(GetTestConciergeClient()->start_arc_vm_call_count(), 0);

  EXPECT_EQ(GetTestConciergeClient()->create_disk_image_call_count(), 1);
}

TEST_F(ArcVmClientAdapterTest, VirtioBlkForData_CreateDiskImageStatusFailed) {
  GetTestConciergeClient()->set_create_disk_image_response(
      CreateDiskImageResponse(vm_tools::concierge::DISK_STATUS_FAILED));

  StartParams start_params(GetPopulatedStartParams());
  start_params.use_virtio_blk_data = true;

  // StartArcVm should NOT be called.
  StartMiniArcWithParams(false, std::move(start_params));
  EXPECT_EQ(GetTestConciergeClient()->start_arc_vm_call_count(), 0);

  EXPECT_EQ(GetTestConciergeClient()->create_disk_image_call_count(), 1);
}

TEST_F(ArcVmClientAdapterTest, VirtioBlkForData_CreateDiskImageStatusCreated) {
  GetTestConciergeClient()->set_create_disk_image_response(
      CreateDiskImageResponse(vm_tools::concierge::DISK_STATUS_CREATED));

  StartParams start_params(GetPopulatedStartParams());
  start_params.use_virtio_blk_data = true;
  StartMiniArcWithParams(true, std::move(start_params));
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);

  EXPECT_EQ(GetTestConciergeClient()->create_disk_image_call_count(), 1);

  // StartArcVmRequest should contain a disk path created by CreateDiskImage().
  const auto& req = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(HasDiskImage(req, kCreatedDiskImagePath));
  EXPECT_TRUE(req.enable_virtio_blk_data());
}

TEST_F(ArcVmClientAdapterTest, VirtioBlkForData_CreateDiskImageStatusExists) {
  GetTestConciergeClient()->set_create_disk_image_response(
      CreateDiskImageResponse(vm_tools::concierge::DISK_STATUS_EXISTS));

  StartParams start_params = GetPopulatedStartParams();
  start_params.use_virtio_blk_data = true;
  StartMiniArcWithParams(true, std::move(start_params));
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);

  EXPECT_EQ(GetTestConciergeClient()->create_disk_image_call_count(), 1);

  // StartArcVmRequest should contain a disk path created by CreateDiskImage().
  const auto& req = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(HasDiskImage(req, kCreatedDiskImagePath));
  EXPECT_TRUE(req.enable_virtio_blk_data());
}

TEST_F(ArcVmClientAdapterTest, VirtioBlkForData_LvmSupported) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(arc::kLvmApplicationContainers);

  StartParams start_params(GetPopulatedStartParams());
  start_params.use_virtio_blk_data = true;
  StartMiniArcWithParams(true, std::move(start_params));
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);

  // CreateDiskImage() should NOT be called.
  EXPECT_EQ(GetTestConciergeClient()->create_disk_image_call_count(), 0);

  // StartArcVmRequest should contain the LVM-provided disk path.
  const auto& req = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(req.enable_virtio_blk_data());
  const std::string expected_lvm_disk_path =
      base::StringPrintf("/dev/mapper/vm/dmcrypt-%s-arcvm",
                         std::string(kUserIdHash).substr(0, 8).c_str());
  const auto& disks = req.disks();
  auto it =
      base::ranges::find_if(disks, [&expected_lvm_disk_path](const auto& disk) {
        return disk.path() == expected_lvm_disk_path;
      });
  EXPECT_NE(it, disks.end());
  // O_DIRECT option should always be enabled on LVM-provided disk images.
  EXPECT_TRUE(it->o_direct());
}

TEST_F(ArcVmClientAdapterTest, VirtioBlkForData_OverrideUseLvm) {
  // ArcVirtioBlkDataConfigOverride:use_lvm/true should override
  // ArcLvmApplicationContainers flag.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{arc::kVirtioBlkDataConfigOverride,
                             {{"use_lvm", "true"}}}},
      /*disabled_features=*/{arc::kLvmApplicationContainers});

  StartParams start_params(GetPopulatedStartParams());
  start_params.use_virtio_blk_data = true;
  StartMiniArcWithParams(true, std::move(start_params));
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);

  // CreateDiskImage() should NOT be called.
  EXPECT_EQ(GetTestConciergeClient()->create_disk_image_call_count(), 0);

  // StartArcVmRequest should contain the LVM-provided disk path.
  const auto& req = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(req.enable_virtio_blk_data());
  const std::string expected_lvm_disk_path =
      base::StringPrintf("/dev/mapper/vm/dmcrypt-%s-arcvm",
                         std::string(kUserIdHash).substr(0, 8).c_str());
  const auto& disks = req.disks();
  auto it =
      base::ranges::find_if(disks, [&expected_lvm_disk_path](const auto& disk) {
        return disk.path() == expected_lvm_disk_path;
      });
  EXPECT_NE(it, disks.end());
  // O_DIRECT option should always be enabled on LVM-provided disk images.
  EXPECT_TRUE(it->o_direct());
}

TEST_F(ArcVmClientAdapterTest, VirtioBlkForData_NoLvmForEphemeralCryptohome) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(arc::kLvmApplicationContainers);

  // Set the guest account, whose cryptohome data is ephemeral.
  SetAccountId(user_manager::GuestAccountId());

  GetTestConciergeClient()->set_create_disk_image_response(
      CreateDiskImageResponse(vm_tools::concierge::DISK_STATUS_CREATED));

  StartParams start_params(GetPopulatedStartParams());
  start_params.use_virtio_blk_data = true;
  StartMiniArcWithParams(true, std::move(start_params));
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);

  EXPECT_EQ(GetTestConciergeClient()->create_disk_image_call_count(), 1);

  // LVM shouldn't be used for ephemeral user even if LvmApplicationContainers
  // feature is enabled.
  const auto& req = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(HasDiskImage(req, kCreatedDiskImagePath));
  EXPECT_TRUE(req.enable_virtio_blk_data());
}

TEST_F(ArcVmClientAdapterTest, DataBlockIoScheduler_Enabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{arc::kBlockIoScheduler, {{"data_block_io_scheduler", "true"}}}}, {});

  StartParams start_params(GetPopulatedStartParams());
  start_params.use_virtio_blk_data = true;

  StartMiniArcWithParams(true, std::move(start_params));

  const auto& req = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(req.enable_data_block_io_scheduler());
}

TEST_F(ArcVmClientAdapterTest, DataBlockIoScheduler_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      // Disabled.
      {{arc::kBlockIoScheduler, {{"data_block_io_scheduler", "false"}}}}, {});

  StartParams start_params(GetPopulatedStartParams());
  start_params.use_virtio_blk_data = true;

  StartMiniArcWithParams(true, std::move(start_params));

  const auto& req = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(req.enable_data_block_io_scheduler());
}

TEST_F(ArcVmClientAdapterTest, DataBlockIoScheduler_VirtioBlkDataIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{arc::kBlockIoScheduler, {{"data_block_io_scheduler", "true"}}}}, {});

  StartParams start_params(GetPopulatedStartParams());
  // virtio-blk /data is disabled.
  start_params.use_virtio_blk_data = false;

  StartMiniArcWithParams(true, std::move(start_params));

  const auto& req = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(req.enable_data_block_io_scheduler());
}

TEST_F(ArcVmClientAdapterTest, DataBlockIoScheduler_DisabledForLvm) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      // Uses LVM.
      {{arc::kLvmApplicationContainers, {}},
       {arc::kBlockIoScheduler, {{"data_block_io_scheduler", "true"}}}},
      {});

  StartParams start_params(GetPopulatedStartParams());
  start_params.use_virtio_blk_data = true;

  StartMiniArcWithParams(true, std::move(start_params));

  const auto& req = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(req.enable_data_block_io_scheduler());
}

TEST_F(ArcVmClientAdapterTest, MetadataDisk_DisabledForArcT) {
  // Metadata disk should not be requested for ARC T.
  base::test::ScopedChromeOSVersionInfo version(
      "CHROMEOS_ARC_ANDROID_SDK_VERSION=33", base::Time::Now());

  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& req = GetTestConciergeClient()->start_arc_vm_request();

  const std::string metadta_disk_path =
      base::StringPrintf("/run/daemon-store/crosvm/%s/YXJjdm0=.metadata.img",
                         std::string(kUserIdHash).c_str());
  EXPECT_FALSE(HasDiskImage(req, metadta_disk_path));
}

TEST_F(ArcVmClientAdapterTest, MetadataDisk_EnabledForArcU) {
  // Metadata disk should be requested for ARC U.
  base::test::ScopedChromeOSVersionInfo version(
      "CHROMEOS_ARC_ANDROID_SDK_VERSION=34", base::Time::Now());

  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& req = GetTestConciergeClient()->start_arc_vm_request();

  const std::string metadta_disk_path =
      base::StringPrintf("/run/daemon-store/crosvm/%s/YXJjdm0=.metadata.img",
                         std::string(kUserIdHash).c_str());
  EXPECT_TRUE(HasDiskImage(req, metadta_disk_path));
}

TEST_F(ArcVmClientAdapterTest, SyspropDiskAlwaysEnabled) {
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();

  const std::string sysprop_disk_path =
      base::StringPrintf("/run/daemon-store/crosvm/%s/YXJjdm0=.runtime.prop",
                         std::string(kUserIdHash).c_str());
  EXPECT_TRUE(HasDiskImage(request, sysprop_disk_path));
  EXPECT_EQ(request.disks(5).path(), sysprop_disk_path);
}

TEST_F(ArcVmClientAdapterTest, ArcErofsImagesDisabled) {
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(request.rootfs_o_direct());  // system image
  EXPECT_FALSE(request.disks(0).o_direct());  // vendor image
}

TEST_F(ArcVmClientAdapterTest, ArcErofsImagesEnabled) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-erofs"});
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(request.rootfs_o_direct());  // system image
  EXPECT_TRUE(request.disks(0).o_direct());  // vendor image
}

// Tests that the binary translation type is set to None when no library is
// enabled by USE flags.
TEST_F(ArcVmClientAdapterTest, BinaryTranslationTypeNone) {
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(
      request.native_bridge_experiment(),
      vm_tools::concierge::StartArcVmRequest::BINARY_TRANSLATION_TYPE_NONE);
}

// Tests that the binary translation type is set to Houdini when only 32-bit
// Houdini library is enabled by USE flags.
TEST_F(ArcVmClientAdapterTest, BinaryTranslationTypeHoudini) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--enable-houdini"});
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(
      request.native_bridge_experiment(),
      vm_tools::concierge::StartArcVmRequest::BINARY_TRANSLATION_TYPE_HOUDINI);
}

// Tests that the binary translation type is set to Houdini when only 64-bit
// Houdini library is enabled by USE flags.
TEST_F(ArcVmClientAdapterTest, BinaryTranslationTypeHoudini64) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--enable-houdini64"});
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(
      request.native_bridge_experiment(),
      vm_tools::concierge::StartArcVmRequest::BINARY_TRANSLATION_TYPE_HOUDINI);
}

// Tests that the binary translation type is set to NDK translation when only
// 32-bit NDK translation library is enabled by USE flags.
TEST_F(ArcVmClientAdapterTest, BinaryTranslationTypeNdkTranslation) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--enable-ndk-translation"});
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(request.native_bridge_experiment(),
            vm_tools::concierge::StartArcVmRequest::
                BINARY_TRANSLATION_TYPE_NDK_TRANSLATION);
}

// Tests that the binary translation type is set to NDK translation when only
// 64-bit NDK translation library is enabled by USE flags.
TEST_F(ArcVmClientAdapterTest, BinaryTranslationTypeNdkTranslation64) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--enable-ndk-translation64"});
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(request.native_bridge_experiment(),
            vm_tools::concierge::StartArcVmRequest::
                BINARY_TRANSLATION_TYPE_NDK_TRANSLATION);
}

// Tests that the binary translation type is set to NDK translation when both
// Houdini and NDK translation libraries are enabled by USE flags, and the
// parameter start_params.native_bridge_experiment is set to true.
TEST_F(ArcVmClientAdapterTest, BinaryTranslationTypeNativeBridgeExperiment) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--enable-houdini", "--enable-ndk-translation"});
  StartParams start_params(GetPopulatedStartParams());
  start_params.native_bridge_experiment = true;
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(request.native_bridge_experiment(),
            vm_tools::concierge::StartArcVmRequest::
                BINARY_TRANSLATION_TYPE_NDK_TRANSLATION);
}

// Tests that the binary translation type is set to Houdini when both Houdini
// and NDK translation libraries are enabled by USE flags, and the parameter
// start_params.native_bridge_experiment is set to false.
TEST_F(ArcVmClientAdapterTest, BinaryTranslationTypeNoNativeBridgeExperiment) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--enable-houdini", "--enable-ndk-translation"});
  StartParams start_params(GetPopulatedStartParams());
  start_params.native_bridge_experiment = false;
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(
      request.native_bridge_experiment(),
      vm_tools::concierge::StartArcVmRequest::BINARY_TRANSLATION_TYPE_HOUDINI);
}

// Tests that the "generate" command line switches the mode.
TEST_F(ArcVmClientAdapterTest, TestGetArcVmUreadaheadModeGenerate) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arcvm-ureadahead-mode=generate"});
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(request.ureadahead_mode(),
            vm_tools::concierge::StartArcVmRequest::UREADAHEAD_MODE_GENERATE);
}

// Tests that the "disabled" command line disables both readahead and generate.
TEST_F(ArcVmClientAdapterTest, TestGetArcVmUreadaheadModeDisabled) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arcvm-ureadahead-mode=disabled"});
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(request.ureadahead_mode(),
            vm_tools::concierge::StartArcVmRequest::UREADAHEAD_MODE_DISABLED);
}

// Tests that ArcVmClientAdapter connects to the boot notification server
// twice: once in StartMiniArc to check that it is listening, and the second
// time in UpgradeArc to send props.
TEST_F(ArcVmClientAdapterTest, TestConnectToBootNotificationServer) {
  StartMiniArc();
  EXPECT_EQ(boot_notification_server()->connection_count(), 1);
  EXPECT_TRUE(boot_notification_server()->received_data().empty());

  UpgradeArcWithParams(/*expect_success=*/true, GetPopulatedUpgradeParams());
  EXPECT_EQ(boot_notification_server()->connection_count(), 2);
  EXPECT_FALSE(boot_notification_server()->received_data().empty());

  // Compare received data to expected output
  std::string expected_props = base::StringPrintf(
      "CID=%" PRId64 "\n%s", kCid,
      base::JoinString(
          GenerateUpgradePropsForTesting(GetPopulatedUpgradeParams(),
                                         kSerialNumber, "ro.boot"),
          "\n")
          .c_str());
  EXPECT_EQ(boot_notification_server()->received_data(), expected_props);
}

// Tests that StartMiniArc fails when the boot notification server is not
// listening.
TEST_F(ArcVmClientAdapterTest, TestBootNotificationServerIsNotListening) {
  boot_notification_server()->Stop();
  // Change timeout to 26 seconds to allow for exponential backoff.
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Seconds(26));

  StartMiniArcWithParams(false, {});
}

// Tests that UpgradeArc() fails when sending the upgrade props
// to the boot notification server fails.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_SendPropFail) {
  StartMiniArc();

  // Let ConnectToArcVmBootNotificationServer() return an invalid FD.
  SetArcVmBootNotificationServerFdForTesting(-1);

  UpgradeArcWithParamsAndStopVmCount(false, {}, /*run_until_stop_vm_count=*/2);
  ExpectArcStopped();
}

// Tests that UpgradeArc() fails when sending the upgrade props
// to the boot notification server fails.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_SendPropFailNotWritable) {
  StartMiniArc();

  // Let ConnectToArcVmBootNotificationServer() return dup(STDIN_FILENO) which
  // is not writable.
  SetArcVmBootNotificationServerFdForTesting(STDIN_FILENO);

  UpgradeArcWithParamsAndStopVmCount(false, {}, /*run_until_stop_vm_count=*/2);
  ExpectArcStopped();
}

TEST_F(ArcVmClientAdapterTest, DisableDownloadProviderDefault) {
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  // Not expected arc_disable_download_provider in properties.
  EXPECT_FALSE(request.mini_instance_request().disable_download_provider());
}

TEST_F(ArcVmClientAdapterTest, DisableDownloadProviderEnforced) {
  StartParams start_params(GetPopulatedStartParams());
  start_params.disable_download_provider = true;
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(request.mini_instance_request().disable_download_provider());
}

TEST_F(ArcVmClientAdapterTest, BroadcastPreANRDefault) {
  StartMiniArc();
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(request.enable_broadcast_anr_prenotify());
}

TEST_F(ArcVmClientAdapterTest, BroadcastPreANREnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(arc::kVmBroadcastPreNotifyANR, true);

  StartMiniArc();
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(request.enable_broadcast_anr_prenotify());
}

TEST_F(ArcVmClientAdapterTest, TrimVmMemory_Success) {
  SetValidUserInfo();
  vm_tools::concierge::ReclaimVmMemoryResponse response;
  response.set_success(true);
  GetTestConciergeClient()->set_reclaim_vm_memory_response(response);

  bool result = false;
  std::string reason("non empty");
  adapter()->TrimVmMemory(
      base::BindLambdaForTesting(
          [&result, &reason](bool success, const std::string& failure_reason) {
            result = success;
            reason = failure_reason;
          }),
      0);
  run_loop()->RunUntilIdle();
  EXPECT_TRUE(result);
  EXPECT_TRUE(reason.empty());
  EXPECT_EQ(GetTestConciergeClient()->reclaim_vm_call_count(), 1);
  EXPECT_EQ(GetTestConciergeClient()->reclaim_vm_request().page_limit(), 0);
}

TEST_F(ArcVmClientAdapterTest, TrimVmMemory_LimitPagesHonored) {
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  UpgradeArc(true);

  vm_tools::concierge::ReclaimVmMemoryResponse response;
  response.set_success(true);
  GetTestConciergeClient()->set_reclaim_vm_memory_response(response);

  bool result = false;
  std::string reason("non empty");
  adapter()->TrimVmMemory(
      base::BindLambdaForTesting(
          [&result, &reason](bool success, const std::string& failure_reason) {
            result = success;
            reason = failure_reason;
          }),
      1234);
  run_loop()->RunUntilIdle();
  EXPECT_TRUE(result);
  EXPECT_TRUE(reason.empty());

  // Verify that exactly one call was done to underlying layer, and that
  // the specified parameter value was passed in.
  EXPECT_EQ(GetTestConciergeClient()->reclaim_vm_call_count(), 1);
  EXPECT_EQ(GetTestConciergeClient()->reclaim_vm_request().page_limit(), 1234);

  StopArcInstance();
}

TEST_F(ArcVmClientAdapterTest, TrimVmMemory_Failure) {
  SetValidUserInfo();

  constexpr const char kReason[] = "This is the reason";
  vm_tools::concierge::ReclaimVmMemoryResponse response;
  response.set_success(false);
  response.set_failure_reason(kReason);
  GetTestConciergeClient()->set_reclaim_vm_memory_response(response);

  bool result = true;
  std::string reason;
  adapter()->TrimVmMemory(
      base::BindLambdaForTesting(
          [&result, &reason](bool success, const std::string& failure_reason) {
            result = success;
            reason = failure_reason;
          }),
      0);
  run_loop()->RunUntilIdle();
  EXPECT_FALSE(result);
  EXPECT_EQ(kReason, reason);
}

TEST_F(ArcVmClientAdapterTest, TrimVmMemory_EmptyResponse) {
  SetValidUserInfo();

  // By default, the fake concierge client returns an empty response.
  // This is to make sure TrimMemoty() can handle such a response.
  bool result = true;
  std::string reason;
  adapter()->TrimVmMemory(
      base::BindLambdaForTesting(
          [&result, &reason](bool success, const std::string& failure_reason) {
            result = success;
            reason = failure_reason;
          }),
      0);
  run_loop()->RunUntilIdle();
  EXPECT_FALSE(result);
  EXPECT_FALSE(reason.empty());
}

TEST_F(ArcVmClientAdapterTest, TrimVmMemory_EmptyUserIdHash) {
  adapter()->SetUserInfo(cryptohome::Identification(), std::string(),
                         std::string());

  constexpr const char kReason[] = "This is the reason";
  vm_tools::concierge::ReclaimVmMemoryResponse response;
  response.set_success(false);
  response.set_failure_reason(kReason);
  GetTestConciergeClient()->set_reclaim_vm_memory_response(response);

  bool result = true;
  std::string reason;
  adapter()->TrimVmMemory(
      base::BindLambdaForTesting(
          [&result, &reason](bool success, const std::string& failure_reason) {
            result = success;
            reason = failure_reason;
          }),
      0);
  run_loop()->RunUntilIdle();
  EXPECT_FALSE(result);
  // When |user_id_hash_| is empty, the call will fail without talking to
  // Concierge.
  EXPECT_NE(kReason, reason);
  EXPECT_FALSE(reason.empty());
}

TEST_F(ArcVmClientAdapterTest, ArcVmUseHugePagesEnabled) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arcvm-use-hugepages"});
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(request.use_hugepages());
}

TEST_F(ArcVmClientAdapterTest, ArcVmLockGuestMemoryEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kLockGuestMemory);
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(request.lock_guest_memory());
}

TEST_F(ArcVmClientAdapterTest, ArcVmMemoryOptionsDisabled) {
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  // Verify that both options are disabled by default.
  EXPECT_FALSE(request.use_hugepages());
  EXPECT_FALSE(request.lock_guest_memory());
}

// Test that StartArcVmRequest has no memory_mib field when kVmMemorySize is
// disabled.
TEST_F(ArcVmClientAdapterTest, ArcVmMemorySizeDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kVmMemorySize);
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(request.memory_mib(), kMinVmMemorySizeMiB);
}

// Test that StartArcVmRequest has `memory_mib == system memory` when
// kVmMemorySize is enabled with no maximum and shift_mib := 0.
TEST_F(ArcVmClientAdapterTest, ArcVmMemorySizeEnabledBig) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["shift_mib"] = "0";
  feature_list.InitAndEnableFeatureWithParameters(kVmMemorySize, params);
  base::SystemMemoryInfoKB info;
  ASSERT_TRUE(base::GetSystemMemoryInfo(&info));
  const uint32_t total_mib = info.total / 1024;
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(request.memory_mib(), total_mib);
}

// Test that StartArcVmRequest has `memory_mib == system memory - 1024` when
// kVmMemorySize is enabled with no maximum and shift_mib := -1024.
TEST_F(ArcVmClientAdapterTest, ArcVmMemorySizeEnabledSmall) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["shift_mib"] = "-1024";
  feature_list.InitAndEnableFeatureWithParameters(kVmMemorySize, params);
  base::SystemMemoryInfoKB info;
  ASSERT_TRUE(base::GetSystemMemoryInfo(&info));
  const uint32_t total_mib = info.total / 1024;
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(request.memory_mib(), total_mib - 1024);
}

// Test that StartArcVmRequest has memory_mib unset when kVmMemorySize is
// enabled, but the requested size is too low (due to max_mib being lower than
// the safety minimum).
TEST_F(ArcVmClientAdapterTest, ArcVmMemorySizeEnabledLow) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["shift_mib"] = "0";
  params["max_mib"] = "1024";
  feature_list.InitAndEnableFeatureWithParameters(kVmMemorySize, params);
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  // The 1024 max_mib is below the safety cut-off, so we expect
  // memory_mib to be set to the minimum.
  EXPECT_EQ(request.memory_mib(), kMinVmMemorySizeMiB);
}

// Test that StartArcVmRequest has `memory_mib == 3333` when kVmMemorySize is
// enabled with max_mib := 3333.
// NOTE: requires that the test running system has more than 3333 MiB of RAM.
TEST_F(ArcVmClientAdapterTest, ArcVmMemorySizeEnabledMax) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["shift_mib"] = "0";
  params["max_mib"] = "3333";  // Above the minimum cut-off.
  feature_list.InitAndEnableFeatureWithParameters(kVmMemorySize, params);
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(request.memory_mib(), 3333u);
}

// Test that ARCMVM size is set by ram_percentage.
TEST_F(ArcVmClientAdapterTest, ArcVmMemorySizeWithPercentageParam) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["ram_percentage"] = "25";
  feature_list.InitAndEnableFeatureWithParameters(kVmMemorySize, params);
  base::SystemMemoryInfoKB info;
  ASSERT_TRUE(base::GetSystemMemoryInfo(&info));
  const uint32_t total_mib = info.total / 1024;
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  // shift_mib is -500 by default
  EXPECT_EQ(request.memory_mib(), total_mib / 4 - 500);
}

// Test that ARCMVM size is set by both ram_percentage and shift_mib.
TEST_F(ArcVmClientAdapterTest, ArcVmMemorySizeWithPercentageParamAndShiftMiB) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["ram_percentage"] = "25";
  params["shift_mib"] = "-512";
  feature_list.InitAndEnableFeatureWithParameters(kVmMemorySize, params);
  base::SystemMemoryInfoKB info;
  ASSERT_TRUE(base::GetSystemMemoryInfo(&info));
  const uint32_t total_mib = info.total / 1024;
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(request.memory_mib(), total_mib / 4 - 512);
}

// Test that StartArcVmRequest has no memory_mib field when getting system
// memory info fails.
TEST_F(ArcVmClientAdapterTest, ArcVmMemorySizeEnabledNoSystemMemoryInfo) {
  // Inject the failure.
  class TestDelegate : public ArcVmClientAdapterDelegate {
    bool GetSystemMemoryInfo(base::SystemMemoryInfoKB* info) override {
      return false;
    }
  };
  SetArcVmClientAdapterDelegateForTesting(adapter(),
                                          std::make_unique<TestDelegate>());

  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["shift_mib"] = "0";
  feature_list.InitAndEnableFeatureWithParameters(kVmMemorySize, params);
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(request.memory_mib(), kMinVmMemorySizeMiB);
}

// Test that StartArcVmRequest::memory_mib is limited to k32bitVmRamMaxMib when
// crosvm is a 32-bit process.
// TODO(khmel): Remove this once crosvm becomes 64 bit binary on ARM.
TEST_F(ArcVmClientAdapterTest, ArcVmMemorySizeEnabledOn32Bit) {
  class TestDelegate : public ArcVmClientAdapterDelegate {
    bool GetSystemMemoryInfo(base::SystemMemoryInfoKB* info) override {
      // Return a value larger than k32bitVmRamMaxMib to verify that the VM
      // memory size is actually limited.
      info->total = (k32bitVmRamMaxMib + 1000) * 1024;
      return true;
    }
    bool IsCrosvm32bit() override { return true; }
  };
  SetArcVmClientAdapterDelegateForTesting(adapter(),
                                          std::make_unique<TestDelegate>());

  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["shift_mib"] = "0";
  feature_list.InitAndEnableFeatureWithParameters(kVmMemorySize, params);
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();

  EXPECT_EQ(request.memory_mib(), k32bitVmRamMaxMib);
}

// Test that the request passes an empty disk for the demo image
// or the block apex composite disk when they are not present.
// There should be two empty disks (/dev/block/vdc and /dev/block/vdd)
// and they should have path /dev/null.
TEST_F(ArcVmClientAdapterTest, ArcVmEmptyVirtualDisksExist) {
  StartMiniArc();

  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(request.disks(1).path(), "/dev/null");
  EXPECT_EQ(request.disks(2).path(), "/dev/null");
}

// Test that block apex disk path exists when the composite disk payload
// exists.
TEST_F(ArcVmClientAdapterTest, ArcVmBlockApexDiskExists) {
  constexpr const char path[] = "/opt/google/vms/android/apex/payload.img";
  set_block_apex_path(base::FilePath(path));
  StartMiniArc();
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(base::Contains(request.disks(), path,
                             [](const auto& p) { return p.path(); }));
}

// Test that the block apex disk path isn't included when it doesn't exist.
TEST_F(ArcVmClientAdapterTest, ArcVmNoBlockApexDisk) {
  constexpr const char path[] = "/opt/google/vms/android/apex/payload.img";
  StartMiniArc();
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(base::Contains(request.disks(), path,
                              [](const auto& p) { return p.path(); }));
}

// Tests that OnConnectionReady() calls the ArcVmCompleteBoot call D-Bus method.
TEST_F(ArcVmClientAdapterTest, OnConnectionReady) {
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  UpgradeArc(true);

  // This calls ArcVmClientAdapter::OnConnectionReady().
  arc_bridge_service()->app()->SetInstance(app_instance());
  WaitForInstanceReady(arc_bridge_service()->app());

  EXPECT_EQ(1, GetTestConciergeClient()->arcvm_complete_boot_call_count());
}

// Tests that ArcVmCompleteBoot failure won't crash the adapter.
TEST_F(ArcVmClientAdapterTest, OnConnectionReady_ArcVmCompleteBootFailure) {
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  UpgradeArc(true);

  // Inject the failure.
  std::optional<vm_tools::concierge::ArcVmCompleteBootResponse> response;
  response.emplace();
  response->set_result(
      vm_tools::concierge::ArcVmCompleteBootResult::BAD_REQUEST);
  GetTestConciergeClient()->set_arcvm_complete_boot_response(response);

  // This calls ArcVmClientAdapter::OnConnectionReady().
  arc_bridge_service()->app()->SetInstance(app_instance());
  WaitForInstanceReady(arc_bridge_service()->app());

  EXPECT_EQ(1, GetTestConciergeClient()->arcvm_complete_boot_call_count());
}

// Tests that null ArcVmCompleteBoot reply won't crash the adapter.
TEST_F(ArcVmClientAdapterTest,
       OnConnectionReady_ArcVmCompleteBootFailureNullReply) {
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  UpgradeArc(true);

  // Inject the failure.
  GetTestConciergeClient()->set_arcvm_complete_boot_response(std::nullopt);

  // This calls ArcVmClientAdapter::OnConnectionReady().
  arc_bridge_service()->app()->SetInstance(app_instance());
  WaitForInstanceReady(arc_bridge_service()->app());

  EXPECT_EQ(1, GetTestConciergeClient()->arcvm_complete_boot_call_count());
}

TEST_F(ArcVmClientAdapterTest, UpgradeArc_EnableArcNearbyShare_Default) {
  StartMiniArc();
  EXPECT_EQ(boot_notification_server()->connection_count(), 1);
  EXPECT_TRUE(boot_notification_server()->received_data().empty());

  UpgradeArcWithParams(/*expect_success=*/true, GetPopulatedUpgradeParams());
  EXPECT_EQ(boot_notification_server()->connection_count(), 2);
  EXPECT_FALSE(boot_notification_server()->received_data().empty());
  EXPECT_TRUE(base::Contains(boot_notification_server()->received_data(),
                             "ro.boot.enable_arc_nearby_share=1"));
}

TEST_F(ArcVmClientAdapterTest, UpgradeArc_EnableArcNearbyShare_Enabled) {
  StartMiniArc();
  EXPECT_EQ(boot_notification_server()->connection_count(), 1);
  EXPECT_TRUE(boot_notification_server()->received_data().empty());

  UpgradeParams upgrade_params = GetPopulatedUpgradeParams();
  upgrade_params.enable_arc_nearby_share = true;
  UpgradeArcWithParams(/*expect_success=*/true, upgrade_params);
  EXPECT_EQ(boot_notification_server()->connection_count(), 2);
  EXPECT_FALSE(boot_notification_server()->received_data().empty());
  EXPECT_TRUE(base::Contains(boot_notification_server()->received_data(),
                             "ro.boot.enable_arc_nearby_share=1"));
}

TEST_F(ArcVmClientAdapterTest, UpgradeArc_EnableArcNearbyShare_Disabled) {
  StartMiniArc();
  EXPECT_EQ(boot_notification_server()->connection_count(), 1);
  EXPECT_TRUE(boot_notification_server()->received_data().empty());

  UpgradeParams upgrade_params = GetPopulatedUpgradeParams();
  upgrade_params.enable_arc_nearby_share = false;
  UpgradeArcWithParams(/*expect_success=*/true, upgrade_params);
  EXPECT_EQ(boot_notification_server()->connection_count(), 2);
  EXPECT_FALSE(boot_notification_server()->received_data().empty());
  EXPECT_TRUE(base::Contains(boot_notification_server()->received_data(),
                             "ro.boot.enable_arc_nearby_share=0"));
}

TEST_F(ArcVmClientAdapterTest,
       StartArc_EnableConsumerAutoUpdateToggle_Default) {
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(
      request.mini_instance_request().enable_consumer_auto_update_toggle());
}

TEST_F(ArcVmClientAdapterTest,
       StartArc_EnableConsumerAutoUpdateToggle_Enabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ash::features::kConsumerAutoUpdateToggleAllowed);
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(
      request.mini_instance_request().enable_consumer_auto_update_toggle());
}

TEST_F(ArcVmClientAdapterTest,
       StartArc_EnableConsumerAutoUpdateToggle_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      ash::features::kConsumerAutoUpdateToggleAllowed);
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(
      request.mini_instance_request().enable_consumer_auto_update_toggle());
}

TEST_F(ArcVmClientAdapterTest, StartArc_EnablePrivacyHubForChrome_Default) {
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(request.mini_instance_request().enable_privacy_hub_for_chrome());
}

TEST_F(ArcVmClientAdapterTest, StartArc_EnablePrivacyHubForChrome_Enabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kCrosPrivacyHub);
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(request.mini_instance_request().enable_privacy_hub_for_chrome());
}

TEST_F(ArcVmClientAdapterTest, StartArc_EnablePrivacyHubForChrome_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(ash::features::kCrosPrivacyHub);
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(request.mini_instance_request().enable_privacy_hub_for_chrome());
}

TEST_F(ArcVmClientAdapterTest, StartMiniArc_ArcSwitchToKeymint_Default) {
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(request.mini_instance_request().arc_switch_to_keymint());
}

TEST_F(ArcVmClientAdapterTest, StartMiniArc_EnableArcAttestation_Default) {
  StartMiniArc();
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(request.mini_instance_request().enable_arc_attestation());
}

TEST_F(ArcVmClientAdapterTest, StartMiniArc_EnableArcAttestation_Enabled) {
  base::test::ScopedChromeOSVersionInfo version(
      base::StringPrintf("CHROMEOS_ARC_ANDROID_SDK_VERSION=%d",
                         arc::kArcVersionT),
      base::Time::Now());
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureStates(
      {{arc::kEnableArcAttestation, true}, {arc::kSwitchToKeyMintOnT, true}});
  StartMiniArc();

  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(request.mini_instance_request().enable_arc_attestation());
}

TEST_F(ArcVmClientAdapterTest, StartMiniArc_EnableArcAttestation_DisabledOnR) {
  base::test::ScopedChromeOSVersionInfo version(
      base::StringPrintf("CHROMEOS_ARC_ANDROID_SDK_VERSION=%d",
                         arc::kArcVersionR),
      base::Time::Now());
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureStates(
      {{arc::kEnableArcAttestation, true}, {arc::kSwitchToKeyMintOnT, true}});
  StartMiniArc();

  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(request.mini_instance_request().enable_arc_attestation());
}

TEST_F(ArcVmClientAdapterTest, StartMiniArc_EnableArcAttestation_DisabledOnT) {
  base::test::ScopedChromeOSVersionInfo version(
      base::StringPrintf("CHROMEOS_ARC_ANDROID_SDK_VERSION=%d",
                         arc::kArcVersionT),
      base::Time::Now());
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureStates(
      {{arc::kEnableArcAttestation, false}, {arc::kSwitchToKeyMintOnT, true}});
  StartMiniArc();

  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(request.mini_instance_request().enable_arc_attestation());
}

TEST_F(ArcVmClientAdapterTest,
       StartMiniArc_EnableArcAttestation_KeymintDisabled) {
  base::test::ScopedChromeOSVersionInfo version(
      base::StringPrintf("CHROMEOS_ARC_ANDROID_SDK_VERSION=%d",
                         arc::kArcVersionT),
      base::Time::Now());
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(arc::kSwitchToKeyMintOnT, false);
  StartMiniArc();

  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(request.mini_instance_request().enable_arc_attestation());
}

// Test that the value of swappiness is default value when kGuestZram is
// disabled.
TEST_F(ArcVmClientAdapterTest, ArcGuestZramDisabledSwappiness) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kGuestSwap);
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(0, request.guest_swappiness());
}

// Test that StartArcVmRequest has correct swappiness value.
TEST_F(ArcVmClientAdapterTest, ArcGuestZramSwappinessValid) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["swappiness"] = "90";
  params["size"] = base::NumberToString(256 * 1024 * 1024);
  params["size_percentage"] = "0";
  feature_list.InitAndEnableFeatureWithParameters(kGuestSwap, params);
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(90, request.guest_swappiness());
  EXPECT_EQ(256u, request.guest_zram_mib());
}

TEST_F(ArcVmClientAdapterTest, ArcGuestZramSizeByPercentage_5GbSystem) {
  class TestDelegate : public ArcVmClientAdapterDelegate {
    bool GetSystemMemoryInfo(base::SystemMemoryInfoKB* info) override {
      info->total = 5 * 1024 * 1024;
      return true;
    }
    bool IsCrosvm32bit() override { return false; }
  };
  SetArcVmClientAdapterDelegateForTesting(adapter(),
                                          std::make_unique<TestDelegate>());
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["size"] = "2000";  // Should be ignored
  params["size_percentage"] = "50";

  feature_list.InitAndEnableFeatureWithParameters(kGuestSwap, params);
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));

  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  // As shift_mib for memory size is -500 by default,
  // 5GB system should result in 4.5GB VM size => 2.25GB ZRAM.
  EXPECT_EQ(2310u, request.guest_zram_mib());
}

TEST_F(ArcVmClientAdapterTest, ArcGuestZramSizeByPercentage_4GbSystem) {
  class TestDelegate : public ArcVmClientAdapterDelegate {
    bool GetSystemMemoryInfo(base::SystemMemoryInfoKB* info) override {
      info->total = 4 * 1024 * 1024;
      return true;
    }
    bool IsCrosvm32bit() override { return false; }
  };
  SetArcVmClientAdapterDelegateForTesting(adapter(),
                                          std::make_unique<TestDelegate>());
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["size"] = "2000";  // Should be ignored
  params["size_percentage"] = "50";

  feature_list.InitAndEnableFeatureWithParameters(kGuestSwap, params);
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));

  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  // As shift_mib for memory size is -500 by default,
  // 4GB system should result in 3.5GB VM size => 1.75GB ZRAM.
  EXPECT_EQ(1798u, request.guest_zram_mib());
}

TEST_F(ArcVmClientAdapterTest, ArcGuestZramSizeByPercentage_CustomMem) {
  class TestDelegate : public ArcVmClientAdapterDelegate {
    bool GetSystemMemoryInfo(base::SystemMemoryInfoKB* info) override {
      info->total = 6 * 1024 * 1024;
      return true;
    }
    bool IsCrosvm32bit() override { return false; }
  };
  SetArcVmClientAdapterDelegateForTesting(adapter(),
                                          std::make_unique<TestDelegate>());
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;

  feature_list.InitWithFeaturesAndParameters(
      {{kGuestSwap, {{"size_percentage", "50"}}},
       {kVmMemorySize, {{"shift_mib", "-2048"}}}},
      {});
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));

  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  // 6GB system with -2GB shift results in 4GB VM size => 2GB ZRAM.
  EXPECT_EQ(2048u, request.guest_zram_mib());
}

// Test that StartArcVmRequest has no matching command line flag
// when kVmMemoryPSIReports is disabled.
TEST_F(ArcVmClientAdapterTest, ArcVmMemoryPSIReportsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kVmMemoryPSIReports);
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(request.vm_memory_psi_period(), -1);
}

// Test that StartArcVmRequest has correct  command line flag
// when kVmMemoryPSIReports is enabled.
TEST_F(ArcVmClientAdapterTest, ArcVmMemoryPSIReportsEnabled) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["period"] = "300";
  feature_list.InitAndEnableFeatureWithParameters(kVmMemoryPSIReports, params);
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  EXPECT_GE(GetTestConciergeClient()->start_arc_vm_call_count(), 1);
  EXPECT_FALSE(is_system_shutdown().has_value());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(request.vm_memory_psi_period(), 300);
}

struct DalvikMemoryProfileTestParam {
  // Requested profile.
  StartParams::DalvikMemoryProfile profile;
  // Name of profile that is expected.
  const char* profile_name;
  StartArcMiniInstanceRequest::DalvikMemoryProfile arc_profile;
};

constexpr DalvikMemoryProfileTestParam kDalvikMemoryProfileTestCases[] = {
    {StartParams::DalvikMemoryProfile::DEFAULT, "4G",
     StartArcMiniInstanceRequest_DalvikMemoryProfile_MEMORY_PROFILE_DEFAULT},
    {StartParams::DalvikMemoryProfile::M4G, "4G",
     StartArcMiniInstanceRequest_DalvikMemoryProfile_MEMORY_PROFILE_4G},
    {StartParams::DalvikMemoryProfile::M8G, "8G",
     StartArcMiniInstanceRequest_DalvikMemoryProfile_MEMORY_PROFILE_8G},
    {StartParams::DalvikMemoryProfile::M16G, "16G",
     StartArcMiniInstanceRequest_DalvikMemoryProfile_MEMORY_PROFILE_16G}};

class ArcVmClientAdapterDalvikMemoryProfileTest
    : public ArcVmClientAdapterTest,
      public testing::WithParamInterface<DalvikMemoryProfileTestParam> {};

INSTANTIATE_TEST_SUITE_P(All,
                         ArcVmClientAdapterDalvikMemoryProfileTest,
                         ::testing::ValuesIn(kDalvikMemoryProfileTestCases));

TEST_P(ArcVmClientAdapterDalvikMemoryProfileTest, Profile) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(arc::kUseDalvikMemoryProfile,
                                    true /* use */);

  const auto& test_param = GetParam();
  StartParams start_params(GetPopulatedStartParams());
  start_params.dalvik_memory_profile = test_param.profile;
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(request.mini_instance_request().dalvik_memory_profile(),
            test_param.arc_profile);
}

TEST_F(ArcVmClientAdapterTest, ArcVmTTSCachingDefault) {
  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(request.mini_instance_request().enable_tts_caching());
}

TEST_F(ArcVmClientAdapterTest, ArcVmTTSCachingEnabled) {
  StartParams start_params(GetPopulatedStartParams());
  start_params.enable_tts_caching = true;
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(request.mini_instance_request().enable_tts_caching());
}

TEST_F(ArcVmClientAdapterTest, ArcVmLcdDensity) {
  StartParams start_params(GetPopulatedStartParams());
  start_params.lcd_density = 480;
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(480, request.mini_instance_request().lcd_density());
}

TEST_F(ArcVmClientAdapterTest, ArcVmPlayStoreAutoUpdateOn) {
  StartParams start_params(GetPopulatedStartParams());
  start_params.play_store_auto_update =
      StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_ON;
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(StartArcMiniInstanceRequest_PlayStoreAutoUpdate_AUTO_UPDATE_ON,
            request.mini_instance_request().play_store_auto_update());
}

TEST_F(ArcVmClientAdapterTest, ArcVmPlayStoreAutoUpdateOff) {
  StartParams start_params(GetPopulatedStartParams());
  start_params.play_store_auto_update =
      StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_OFF;
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(StartArcMiniInstanceRequest_PlayStoreAutoUpdate_AUTO_UPDATE_OFF,
            request.mini_instance_request().play_store_auto_update());
}

TEST_F(ArcVmClientAdapterTest, ArcVmPlayStoreAutoUpdateDefault) {
  StartParams start_params(GetPopulatedStartParams());
  start_params.play_store_auto_update =
      StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_DEFAULT;
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(StartArcMiniInstanceRequest_PlayStoreAutoUpdate_AUTO_UPDATE_DEFAULT,
            request.mini_instance_request().play_store_auto_update());
}

TEST_F(ArcVmClientAdapterTest, ConvertUpgradeParams_SkipTtsCacheSetup) {
  StartMiniArc();
  UpgradeParams upgrade_params = GetPopulatedUpgradeParams();
  upgrade_params.skip_tts_cache = true;
  UpgradeArcWithParams(true, std::move(upgrade_params));
  EXPECT_TRUE(base::Contains(boot_notification_server()->received_data(),
                             "ro.boot.skip_tts_cache=1"));
}

TEST_F(ArcVmClientAdapterTest, ConvertUpgradeParams_EnableTtsCacheSetup) {
  StartMiniArc();
  UpgradeParams upgrade_params = GetPopulatedUpgradeParams();
  upgrade_params.skip_tts_cache = false;
  UpgradeArcWithParams(true, std::move(upgrade_params));
  EXPECT_TRUE(base::Contains(boot_notification_server()->received_data(),
                             "ro.boot.skip_tts_cache=0"));
}

TEST_F(ArcVmClientAdapterTest, mglruReclaimDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(arc::kMglruReclaim, false);
  StartMiniArcWithParams(true, GetPopulatedStartParams());
  auto req = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(req.mglru_reclaim_interval(), 0);
  EXPECT_EQ(req.mglru_reclaim_swappiness(), 0);
}

TEST_F(ArcVmClientAdapterTest, mglruReclaimEnabled) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["interval"] = "30000";
  params["swappiness"] = "100";
  feature_list.InitAndEnableFeatureWithParameters(kMglruReclaim, params);
  StartMiniArcWithParams(true, GetPopulatedStartParams());
  auto req = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(req.mglru_reclaim_interval(), 30000);
  EXPECT_EQ(req.mglru_reclaim_swappiness(), 100);
}

TEST_F(ArcVmClientAdapterTest, LazyWebViewInitEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(kEnableLazyWebViewInit, true);
  StartParams start_params(GetPopulatedStartParams());

  StartMiniArcWithParams(true, std::move(start_params));

  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(request.enable_web_view_zygote_lazy_init());
}

TEST_F(ArcVmClientAdapterTest, LazyWebViewInitDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(kEnableLazyWebViewInit, false);
  StartParams start_params(GetPopulatedStartParams());

  StartMiniArcWithParams(true, std::move(start_params));

  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(request.enable_web_view_zygote_lazy_init());
}

TEST_F(ArcVmClientAdapterTest, ArcFilePickerExperimentFalse) {
  StartParams start_params(GetPopulatedStartParams());
  start_params.arc_file_picker_experiment = false;
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(request.mini_instance_request().arc_file_picker_experiment());
}

TEST_F(ArcVmClientAdapterTest, ArcFilePickerExperimentTrue) {
  StartParams start_params(GetPopulatedStartParams());
  start_params.arc_file_picker_experiment = true;
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(request.mini_instance_request().arc_file_picker_experiment());
}

TEST_F(ArcVmClientAdapterTest, ArcCustomTabsExperimentFalse) {
  StartParams start_params(GetPopulatedStartParams());
  start_params.arc_custom_tabs_experiment = false;
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(request.mini_instance_request().arc_custom_tabs_experiment());
}

TEST_F(ArcVmClientAdapterTest, ArcCustomTabsExperimentTrue) {
  StartParams start_params(GetPopulatedStartParams());
  start_params.arc_custom_tabs_experiment = true;
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(request.mini_instance_request().arc_custom_tabs_experiment());
}

TEST_F(ArcVmClientAdapterTest, StartMiniArc_ArcSignedIn) {
  StartParams start_params(GetPopulatedStartParams());
  start_params.arc_signed_in = true;
  StartMiniArcWithParams(true, std::move(start_params));
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(request.mini_instance_request().arc_signed_in());
}

TEST_F(ArcVmClientAdapterTest, StartMiniArc_ArcSignedInDisabled) {
  StartMiniArcWithParams(true, GetPopulatedStartParams());
  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(request.mini_instance_request().arc_signed_in());
}

TEST_F(ArcVmClientAdapterTest, ArcPriorityAppLmkDelayDisabled) {
  StartMiniArc();
  UpgradeParams upgrade_params = GetPopulatedUpgradeParams();
  upgrade_params.enable_priority_app_lmk_delay = false;
  UpgradeArcWithParams(true, std::move(upgrade_params));
  EXPECT_FALSE(base::Contains(boot_notification_server()->received_data(),
                              "ro.boot.arc.lmk.enable_priority_app_delay"));
  EXPECT_FALSE(
      base::Contains(boot_notification_server()->received_data(),
                     "ro.boot.arc.lmk.priority_app_delay_duration_sec"));
  EXPECT_FALSE(base::Contains(boot_notification_server()->received_data(),
                              "ro.boot.arc.lmk.priority_apps"));
}

TEST_F(ArcVmClientAdapterTest, ArcPriorityAppLmkDelayEnabled_NoApp) {
  StartMiniArc();
  UpgradeParams upgrade_params = GetPopulatedUpgradeParams();
  upgrade_params.enable_priority_app_lmk_delay = true;
  upgrade_params.priority_app_lmk_delay_list = "";
  UpgradeArcWithParams(true, std::move(upgrade_params));
  EXPECT_FALSE(base::Contains(boot_notification_server()->received_data(),
                              "ro.boot.arc.lmk.enable_priority_app_delay"));
  EXPECT_FALSE(
      base::Contains(boot_notification_server()->received_data(),
                     "ro.boot.arc.lmk.priority_app_delay_duration_sec"));
  EXPECT_FALSE(base::Contains(boot_notification_server()->received_data(),
                              "ro.boot.arc.lmk.priority_apps"));
}

TEST_F(ArcVmClientAdapterTest, ArcPriorityAppLmkDelayEnabled_SomeApp) {
  StartMiniArc();
  UpgradeParams upgrade_params = GetPopulatedUpgradeParams();
  upgrade_params.enable_priority_app_lmk_delay = true;
  upgrade_params.priority_app_lmk_delay_list =
      "com.example.app1,com.example.app2";
  upgrade_params.priority_app_lmk_delay_second = 60;
  UpgradeArcWithParams(true, std::move(upgrade_params));
  EXPECT_TRUE(base::Contains(boot_notification_server()->received_data(),
                             "ro.boot.arc.lmk.enable_priority_app_delay=1"));
  EXPECT_TRUE(base::Contains(
      boot_notification_server()->received_data(),
      "ro.boot.arc.lmk.priority_apps=com.example.app1,com.example.app2"));
  EXPECT_TRUE(
      base::Contains(boot_notification_server()->received_data(),
                     "ro.boot.arc.lmk.priority_app_delay_duration_sec=60"));
}

TEST_F(ArcVmClientAdapterTest, ArcLmkPerceptibleMinStateUpdateDisabled) {
  StartMiniArc();
  UpgradeParams upgrade_params = GetPopulatedUpgradeParams();
  upgrade_params.enable_lmk_perceptible_min_state_update = false;
  UpgradeArcWithParams(true, std::move(upgrade_params));
  EXPECT_FALSE(base::Contains(boot_notification_server()->received_data(),
                              "ro.boot.arc.lmk.perceptible_min_state_update"));
}

TEST_F(ArcVmClientAdapterTest, ArcLmkPerceptibleMinStateUpdateEnabled) {
  StartMiniArc();
  UpgradeParams upgrade_params = GetPopulatedUpgradeParams();
  upgrade_params.enable_lmk_perceptible_min_state_update = true;
  UpgradeArcWithParams(true, std::move(upgrade_params));
  EXPECT_TRUE(base::Contains(boot_notification_server()->received_data(),
                             "ro.boot.arc.lmk.perceptible_min_state_update=1"));
}

TEST_F(ArcVmClientAdapterTest, DefaultDexOptCacheSetup) {
  StartMiniArc();
  UpgradeParams upgrade_params = GetPopulatedUpgradeParams();
  upgrade_params.skip_tts_cache = false;
  UpgradeArcWithParams(true, std::move(upgrade_params));
  EXPECT_FALSE(base::Contains(boot_notification_server()->received_data(),
                              "ro.boot.skip_dexopt_cache"));
}

TEST_F(ArcVmClientAdapterTest, SkipDexOptCacheSetupArcT) {
  base::test::ScopedChromeOSVersionInfo version(
      "CHROMEOS_ARC_ANDROID_SDK_VERSION=33", base::Time::Now());
  StartMiniArc();
  UpgradeParams upgrade_params = GetPopulatedUpgradeParams();
  upgrade_params.skip_dexopt_cache = true;
  UpgradeArcWithParams(true, std::move(upgrade_params));
  EXPECT_TRUE(base::Contains(boot_notification_server()->received_data(),
                             "ro.boot.skip_dexopt_cache=1"));
}

TEST_F(ArcVmClientAdapterTest, SkipDexOptCacheSetupArcR) {
  base::test::ScopedChromeOSVersionInfo version(
      "CHROMEOS_ARC_ANDROID_SDK_VERSION=30", base::Time::Now());
  StartMiniArc();
  UpgradeParams upgrade_params = GetPopulatedUpgradeParams();
  upgrade_params.skip_dexopt_cache = true;
  UpgradeArcWithParams(true, std::move(upgrade_params));
  EXPECT_FALSE(base::Contains(boot_notification_server()->received_data(),
                              "ro.boot.skip_dexopt_cache"));
}

TEST_F(ArcVmClientAdapterTest, VirtualSwapDevice_Enabled) {
  base::FieldTrialParams params;
  params["size"] = base::NumberToString(256 * 1024 * 1024);
  params["virtual_swap_enabled"] = "true";
  params["virtual_swap_interval_ms"] = "1000";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(kGuestSwap, params);

  StartParams start_params(GetPopulatedStartParams());
  StartMiniArcWithParams(true, std::move(start_params));

  const auto& request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ(0u, request.guest_zram_mib());
  EXPECT_EQ(1000u, request.virtual_swap_config().swap_interval_ms());
  EXPECT_EQ(256u, request.virtual_swap_config().size_mib());
}

}  // namespace
}  // namespace arc
