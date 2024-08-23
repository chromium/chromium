// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "ash/hud_display/ash_tracing_handler.h"
#include "ash/hud_display/ash_tracing_request.h"
#include "ash/hud_display/hud_display.h"
#include "ash/hud_display/hud_settings_view.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/ash/shell_delegate/chrome_shell_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_test.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

namespace ash {
namespace {

// Two global registries track test hud_display::AshTraceDestinationIO and
// test perfetto sessions.
class TestAshTraceDestinationIORegistry;
class TestTracingSessionRegistry;
static TestAshTraceDestinationIORegistry*
    test_ash_trace_destination_io_registry = nullptr;
static TestTracingSessionRegistry* test_tracing_session_registry{nullptr};

std::unique_ptr<hud_display::AshTraceDestinationIO>
CreateTestAshTraceDestinationIO();

const char* AshTracingRequestStatus2String(
    const hud_display::AshTracingRequest::Status& status) {
  switch (status) {
    case hud_display::AshTracingRequest::Status::kEmpty:
      return "kEmpty";
    case hud_display::AshTracingRequest::Status::kInitialized:
      return "kInitialized";
    case hud_display::AshTracingRequest::Status::kStarted:
      return "kStarted";
    case hud_display::AshTracingRequest::Status::kStopping:
      return "kStopping";
    case hud_display::AshTracingRequest::Status::kPendingMount:
      return "kPendingMount";
    case hud_display::AshTracingRequest::Status::kWritingFile:
      return "kWritingFile";
    case hud_display::AshTracingRequest::Status::kCompleted:
      return "kCompleted";
  }
}

// Tracks all hud_display::AshTraceDestinationIO objects.
class TestAshTraceDestinationIORegistry {
 public:
  struct IOStatus {
    base::FilePath tracing_directory_created;
    base::FilePath tracing_file_created;
    bool memfd_created{false};
    bool sendfile_ok{false};
  };

  TestAshTraceDestinationIORegistry() : id_(base::RandUint64()) {}

  TestAshTraceDestinationIORegistry(const TestAshTraceDestinationIORegistry&) =
      delete;
  TestAshTraceDestinationIORegistry& operator=(
      const TestAshTraceDestinationIORegistry&) = delete;

  ~TestAshTraceDestinationIORegistry() = default;

  // CreateTestAshTraceDestinationIO() can be called on the ThreadPool, so we
  // use lock to keep it running.
  IOStatus* NewIOStatus() {
    base::AutoLock l(lock_);
    sessions_.push_back(std::make_unique<IOStatus>());
    return sessions_.back().get();
  }

  // This is used for individual DestinationIO object to check that registry
  // was not replaced (when tests are run the ine same process).
  uint64_t id() const { return id_; }

  const std::vector<std::unique_ptr<IOStatus>>& sessions() const {
    return sessions_;
  }

 private:
  const unsigned id_;
  std::vector<std::unique_ptr<IOStatus>> sessions_;
  base::Lock lock_;
};

// Test hud_display::AshTraceDestinationIO object.
class TestAshTraceDestinationIO : public hud_display::AshTraceDestinationIO {
 public:
  explicit TestAshTraceDestinationIO(
      TestAshTraceDestinationIORegistry* registry)
      : registry_(registry),
        registry_id_(registry->id()),
        status_(registry->NewIOStatus()) {
    AssertRegistry();
  }
  TestAshTraceDestinationIO(const TestAshTraceDestinationIO&) = delete;
  TestAshTraceDestinationIO& operator=(const TestAshTraceDestinationIO&) =
      delete;

  ~TestAshTraceDestinationIO() override = default;

  // Overrides base::CreateDirectory.
  bool CreateDirectory(const base::FilePath& path) override {
    LOG(INFO) << "TestAshTraceDestinationIO::CreateDirectory(path="
              << path.value() << ")";
    AssertRegistry();
    status_->tracing_directory_created = path;
    return true;
  }

  std::tuple<base::File, bool> CreateTracingFile(
      const base::FilePath& path) override {
    LOG(INFO) << "TestAshTraceDestinationIO::CreateTracingFile(path="
              << path.value() << ")";
    AssertRegistry();
    status_->tracing_file_created = path;
    return std::make_tuple(base::File(), true);
  }

  std::tuple<base::PlatformFile, bool> CreateMemFD(
      const char* name,
      unsigned int flags) override {
    LOG(INFO) << "TestAshTraceDestinationIO::CreateMemFD(name=" << name
              << ", flags=" << flags << ")";
    AssertRegistry();
    status_->memfd_created = true;
    return std::make_tuple(base::kInvalidPlatformFile, true);
  }

  bool CanWriteFile(base::PlatformFile fd) override {
    AssertRegistry();
    return status_->memfd_created || !status_->tracing_file_created.empty();
  }

  int fstat(base::PlatformFile fd, struct stat* statbuf) override {
    LOG(INFO) << "TestAshTraceDestinationIO::fstat(): Called.";
    AssertRegistry();
    memset(statbuf, 0, sizeof(struct stat));
    return CanWriteFile(fd) ? 0 : -1;
  }

  ssize_t sendfile(base::PlatformFile out_fd,
                   base::PlatformFile in_fd,
                   off_t* offset,
                   size_t size) override {
    LOG(INFO) << "TestAshTraceDestinationIO::sendfile(): Called.";
    AssertRegistry();
    status_->sendfile_ok = CanWriteFile(out_fd);
    // Should number of bytes written. fstat() from above reports 0 size.
    return status_->sendfile_ok ? 0 : -1;
  }

  const TestAshTraceDestinationIORegistry::IOStatus* status() const {
    return status_;
  }

 private:
  void AssertRegistry() {
    CHECK_EQ(test_ash_trace_destination_io_registry, registry_);
    CHECK_EQ(test_ash_trace_destination_io_registry->id(), registry_id_);
  }

  raw_ptr<const TestAshTraceDestinationIORegistry, LeakedDanglingUntriaged>
      registry_;
  const uint64_t registry_id_;

  raw_ptr<TestAshTraceDestinationIORegistry::IOStatus, LeakedDanglingUntriaged>
      status_;
};

// Keeps track of all test TracingSession objects.
class TestTracingSessionRegistry {
 public:
  struct SessionStatus {
    enum class Status {
      kCreated,
      kConfigured,
      kStarted,
      kStopped,
      kDestroyed,
    };
    Status status;
  };

  TestTracingSessionRegistry() : id_(base::RandUint64()) {}

  TestTracingSessionRegistry(const TestTracingSessionRegistry&) = delete;
  TestTracingSessionRegistry& operator=(const TestTracingSessionRegistry&) =
      delete;

  ~TestTracingSessionRegistry() = default;

  SessionStatus* NewSessionStatus() {
    sessions_.push_back(std::make_unique<SessionStatus>());
    return sessions_.back().get();
  }

  // This is used for individual TracingSession object to check that registry
  // was not replaced (when tests are run the ine same process).
  uint64_t id() const { return id_; }

  const std::vector<std::unique_ptr<SessionStatus>>& sessions() const {
    return sessions_;
  }

 private:
  const unsigned id_;

  std::vector<std::unique_ptr<SessionStatus>> sessions_;
};

// Fake perfetto::TracingSession.
class TestTracingSession : public perfetto::TracingSession {
 public:
  explicit TestTracingSession(TestTracingSessionRegistry* registry)
      : registry_(registry),
        registry_id_(registry->id()),
        status_(registry->NewSessionStatus()) {
    AssertRegistry();
    status_->status =
        TestTracingSessionRegistry::SessionStatus::Status::kCreated;
  }

  ~TestTracingSession() override {
    AssertRegistry();
    CHECK_EQ(status_->status,
             TestTracingSessionRegistry::SessionStatus::Status::kStopped);
    status_->status =
        TestTracingSessionRegistry::SessionStatus::Status::kDestroyed;
  }

  void Setup(const perfetto::TraceConfig&, int fd = -1) override {
    AssertRegistry();
    CHECK_EQ(status_->status,
             TestTracingSessionRegistry::SessionStatus::Status::kCreated);
    status_->status =
        TestTracingSessionRegistry::SessionStatus::Status::kConfigured;
  }

  void Start() override {
    AssertRegistry();
    CHECK_EQ(status_->status,
             TestTracingSessionRegistry::SessionStatus::Status::kConfigured);
    status_->status =
        TestTracingSessionRegistry::SessionStatus::Status::kStarted;
    // perfetto::TracingSession runs callbacks from its own background thread.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(
            [](std::function<void()> on_start_callback) {  // nocheck
              on_start_callback();
            },
            on_start_callback_));
  }

  void StartBlocking() override { NOTIMPLEMENTED(); }

  void SetOnStartCallback(std::function<void()> on_start) override {  // nocheck
    on_start_callback_ = on_start;
  }

  void SetOnErrorCallback(
      std::function<void(perfetto::TracingError)>) override {  // nocheck
    NOTIMPLEMENTED();
  }

  void Flush(std::function<void(bool)>, uint32_t timeout_ms = 0)  // nocheck
      override {
    NOTIMPLEMENTED();
  }

  void Stop() override {
    AssertRegistry();
    CHECK_EQ(status_->status,
             TestTracingSessionRegistry::SessionStatus::Status::kStarted);
    status_->status =
        TestTracingSessionRegistry::SessionStatus::Status::kStopped;
    // perfetto::TracingSession runs callbacks from its own background thread.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(
            [](std::function<void()> on_stop_callback) {  // nocheck
              on_stop_callback();
            },
            on_stop_callback_));
  }

  void StopBlocking() override { NOTIMPLEMENTED(); }

  void SetOnStopCallback(std::function<void()> on_stop) override {  // nocheck
    on_stop_callback_ = on_stop;
  }

  void ChangeTraceConfig(const perfetto::TraceConfig&) override {
    NOTIMPLEMENTED();
  }
  void ReadTrace(ReadTraceCallback) override { NOTIMPLEMENTED(); }
  void GetTraceStats(GetTraceStatsCallback) override { NOTIMPLEMENTED(); }
  void QueryServiceState(QueryServiceStateCallback) override {
    NOTIMPLEMENTED();
  }

 private:
  void AssertRegistry() {
    CHECK_EQ(test_tracing_session_registry, registry_);
    CHECK_EQ(test_tracing_session_registry->id(), registry_id_);
  }

  raw_ptr<const TestTracingSessionRegistry> registry_;
  const uint64_t registry_id_;

  std::function<void()> on_start_callback_;  // nocheck
  std::function<void()> on_stop_callback_;   // nocheck

  raw_ptr<TestTracingSessionRegistry::SessionStatus> status_;
};

// Generates TraceDestination on the ThreadPool (IO-enabled sequence runner)
// and waits for the result.
class TraceDestinationWaiter {
 public:
  TraceDestinationWaiter() = default;

  hud_display::AshTracingRequest::AshTraceDestinationUniquePtr Wait(
      base::Time timestamp) {
    run_loop_ = std::make_unique<base::RunLoop>();
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        hud_display::AshTracingRequest::
            CreateGenerateTraceDestinationTaskForTesting(
                CreateTestAshTraceDestinationIO(), base::Time::Now()),
        base::BindOnce(&TraceDestinationWaiter::OnGenerated,
                       weak_factory_.GetWeakPtr()));
    run_loop_->Run();
    return std::move(destination_);
  }

  void OnGenerated(hud_display::AshTracingRequest::AshTraceDestinationUniquePtr
                       destination) {
    destination_ = std::move(destination);
    run_loop_->Quit();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  hud_display::AshTracingRequest::AshTraceDestinationUniquePtr destination_;

  base::WeakPtrFactory<TraceDestinationWaiter> weak_factory_{this};
};

// Waits for hud_display::TracingManager to receive a known status.
class TestAshTracingManagerObserver
    : public hud_display::AshTracingManager::Observer {
 public:
  using Condition =
      base::RepeatingCallback<bool(hud_display::AshTracingManager&)>;

  explicit TestAshTracingManagerObserver(
      hud_display::AshTracingManager& manager)
      : manager_(manager) {}

  void Wait(Condition condition) {
    if (condition.Run(*manager_)) {
      return;
    }

    condition_ = condition;
    manager_->AddObserver(this);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    manager_->RemoveObserver(this);
  }

  void OnTracingStatusChange() override {
    if (condition_.Run(*manager_)) {
      run_loop_->Quit();
    }
  }

 private:
  const raw_ref<hud_display::AshTracingManager> manager_;
  Condition condition_;

  std::unique_ptr<base::RunLoop> run_loop_;
};

// Waits for the last tracing request to get expected status.
void WaiFortLastTracingRequestStatus(
    hud_display::AshTracingRequest::Status expected_status) {
  LOG(INFO) << "Wait for the last tracing request status = '"
            << AshTracingRequestStatus2String(expected_status) << "'";
  TestAshTracingManagerObserver(hud_display::AshTracingManager::Get())
      .Wait(base::BindRepeating(
          [](hud_display::AshTracingRequest::Status expected_status,
             hud_display::AshTracingManager& manager) {
            if (manager.GetTracingRequestsForTesting().size() == 0)
              return false;

            const hud_display::AshTracingRequest::Status status =
                manager.GetTracingRequestsForTesting().back()->status();

            LOG(INFO) << "Last tracing request status = '"
                      << AshTracingRequestStatus2String(status) << "'";

            return status == expected_status;
          },
          expected_status));
}

// Waits for all except the last one tracing requests to complete.
void WaitForAllButLastTracingRequestsToComplete() {
  LOG(INFO) << "WaitForAllButLastTracingRequestsToComplete(): Waiting.";
  TestAshTracingManagerObserver(hud_display::AshTracingManager::Get())
      .Wait(
          base::BindRepeating([](hud_display::AshTracingManager& manager) {
            if (manager.GetTracingRequestsForTesting().size() < 2)
              return false;

            LOG(INFO)
                << "WaitForAllButLastTracingRequestsToComplete(): There are "
                << manager.GetTracingRequestsForTesting().size()
                << " tracing requests.";
            bool success = true;
            for (size_t i = 0;
                 i < manager.GetTracingRequestsForTesting().size() - 1; ++i) {
              const hud_display::AshTracingRequest::Status status =
                  manager.GetTracingRequestsForTesting()[i]->status();

              LOG(INFO) << "request[" << i + 1 << "/"
                        << manager.GetTracingRequestsForTesting().size()
                        << "] status = '"
                        << AshTracingRequestStatus2String(status) << "'";

              if (status != hud_display::AshTracingRequest::Status::kCompleted)
                success = false;
            }
            return success;
          }));
}

// This is used in
// hud_display::AshTracingHandler::SetPerfettoTracingSessionCreatorForTesting().
std::unique_ptr<perfetto::TracingSession> CreateTestPerfettoSession() {
  return std::make_unique<TestTracingSession>(test_tracing_session_registry);
}

// This is used in
// hud_display::AshTracingRequest::SetAshTraceDestinationIOCreatorForTesting().
std::unique_ptr<hud_display::AshTraceDestinationIO>
CreateTestAshTraceDestinationIO() {
  return std::make_unique<TestAshTraceDestinationIO>(
      test_ash_trace_destination_io_registry);
}

bool GetAshHUDSettingsViewVisible() {
  return hud_display::HUDDisplayView::GetForTesting()
      ->GetSettingsViewForTesting()
      ->GetVisible();
}

void ToggleAshHUDSettingsView() {
  hud_display::HUDDisplayView::GetForTesting()->ToggleSettingsForTesting();
}

void ToggleAshHUDTracing() {
  hud_display::HUDDisplayView::GetForTesting()
      ->GetSettingsViewForTesting()
      ->ToggleTracingForTesting();
}

}  // anonymous namespace

class AshHUDLoginTest
    : public LoginManagerTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  AshHUDLoginTest() : LoginManagerTest() {
    LOG(INFO) << "AshHUDLoginTest: starting test with IsAshDebugFlagSet()="
              << IsAshDebugFlagSet() << ", IsDisableLoggingRedirectFlagSet()="
              << IsDisableLoggingRedirectFlagSet();
    login_manager_mixin_.AppendRegularUsers(1);
  }

  AshHUDLoginTest(const AshHUDLoginTest&) = delete;
  ~AshHUDLoginTest() override = default;

  AshHUDLoginTest& operator=(const AshHUDLoginTest&) = delete;

  bool IsAshDebugFlagSet() const { return std::get<0>(GetParam()); }

  bool IsDisableLoggingRedirectFlagSet() const {
    return std::get<1>(GetParam());
  }

  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_FALSE(test_tracing_session_registry);
    test_tracing_session_registry = new TestTracingSessionRegistry;

    ASSERT_FALSE(test_ash_trace_destination_io_registry);
    test_ash_trace_destination_io_registry =
        new TestAshTraceDestinationIORegistry;

    if (IsAshDebugFlagSet()) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kAshDebugShortcuts);
    }
    ChromeShellDelegate::SetDisableLoggingRedirectForTesting(
        IsDisableLoggingRedirectFlagSet());

    hud_display::AshTracingHandler::SetPerfettoTracingSessionCreatorForTesting(
        CreateTestPerfettoSession);

    hud_display::AshTracingRequest::SetAshTraceDestinationIOCreatorForTesting(
        &CreateTestAshTraceDestinationIO);

    LoginManagerTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    LoginManagerTest::TearDownInProcessBrowserTestFixture();

    hud_display::AshTracingRequest::
        ResetAshTraceDestinationIOCreatorForTesting();

    hud_display::AshTracingHandler::
        ResetPerfettoTracingSessionCreatorForTesting();

    ChromeShellDelegate::ResetDisableLoggingRedirectForTesting();

    if (IsAshDebugFlagSet()) {
      base::CommandLine::ForCurrentProcess()->RemoveSwitch(
          switches::kAshDebugShortcuts);
    }

    delete test_tracing_session_registry;
    test_tracing_session_registry = nullptr;

    delete test_ash_trace_destination_io_registry;
    test_ash_trace_destination_io_registry = nullptr;
  }

  void Login() {
    login_manager_mixin_.SkipPostLoginScreens();

    auto context = LoginManagerMixin::CreateDefaultUserContext(
        login_manager_mixin_.users()[0]);
    login_manager_mixin_.LoginAndWaitForActiveSession(context);
  }

  // This call verifies given hud_display::AshTraceDestination status
  // (via TestAshTraceDestinationIO that keeps track of status in global
  // registry TestAshTraceDestinationIORegistry). Current value of
  // --disable-logging-redirect flag is used from the test parameter, but
  // user login status is given explicitly, because we need to verify
  // "pre-login" status even after login.
  void VerifyTraceDestination(
      const TestAshTraceDestinationIORegistry::IOStatus* destination_status,
      bool user_logged_in) {
    if (IsDisableLoggingRedirectFlagSet()) {
      // Trace file should exist and be a real file in '/run/chrome/tracing/'
      EXPECT_EQ(destination_status->tracing_directory_created.value(),
                "/run/chrome/tracing");
      EXPECT_EQ(destination_status->tracing_file_created.DirName().value(),
                "/run/chrome/tracing");
      EXPECT_FALSE(destination_status->memfd_created);
      return;
    }
    // If DisableLoggingRedirectFlag is not set, destination depends on whether
    // user has logged in.
    if (user_logged_in) {
      // Trace file should exist and be real file in user downloads.
      const base::FilePath folder = Shell::Get()
                                        ->shell_delegate()
                                        ->GetPrimaryUserDownloadsFolder()
                                        .AppendASCII("tracing");
      EXPECT_EQ(destination_status->tracing_directory_created, folder);
      EXPECT_EQ(destination_status->tracing_file_created.DirName(), folder);
      EXPECT_FALSE(destination_status->memfd_created);
      return;
    }
    // DisableLoggingRedirectFlag is not set and user has not logged in.
    // Destination must be memfd.
    EXPECT_TRUE(destination_status->tracing_directory_created.empty());
    EXPECT_TRUE(destination_status->tracing_file_created.empty());
    EXPECT_TRUE(destination_status->memfd_created);
    return;
  }

 protected:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

// See "Testing" section in https://goto.google.com/ash-tracing for details.
IN_PROC_BROWSER_TEST_P(AshHUDLoginTest, AshHUDVerifyTracing) {
  const ui::Accelerator hud_accelerator =
      ui::Accelerator(ui::VKEY_G, kDebugModifier);
  auto trigger_hud = [&]() {
    return ShellTestApi().IsActionForAcceleratorEnabled(hud_accelerator) &&
           ShellTestApi().PressAccelerator(hud_accelerator);
  };

  // Depending on the test parameters, registry grows to different values.
  unsigned expected_io_registry_size = 0;

  // Check that the login screen is shown, but HUD is not.
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_FALSE(ShellTestApi().IsHUDShown());

  // Make sure that Ash HUD can be triggered if and only if
  // --ash-debug-shortcuts flag is set.
  EXPECT_EQ(IsAshDebugFlagSet(), trigger_hud());
  EXPECT_EQ(IsAshDebugFlagSet(), ShellTestApi().IsHUDShown());

  {
    LOG(INFO)
        << "########### Verifying AshTraceDestination generation before login.";
    // Calculate trace destination and verify it.
    //
    // Trace destination should be correct even if HUD UI is not enabled, so
    // we verify it separately.
    auto destination = TraceDestinationWaiter().Wait(base::Time::Now());
    VerifyTraceDestination(
        static_cast<TestAshTraceDestinationIO*>(destination->io())->status(),
        /*user_logged_in=*/false);
    ++expected_io_registry_size;
  }
  if (IsAshDebugFlagSet()) {
    // HUD is visible now.
    // Switch HUD to settings view.
    EXPECT_FALSE(GetAshHUDSettingsViewVisible());
    ToggleAshHUDSettingsView();
    EXPECT_TRUE(GetAshHUDSettingsViewVisible());

    // Start tracing.
    ToggleAshHUDTracing();
    ++expected_io_registry_size;

    // Wait for tracing to start.
    WaiFortLastTracingRequestStatus(
        hud_display::AshTracingRequest::Status::kStarted);

    // Stop tracing.
    ToggleAshHUDTracing();

    // Wait for tracing to stop. Depending on --disable-logging-redirect flag
    // trace may end up Completed or kPendingMount.
    WaiFortLastTracingRequestStatus(
        IsDisableLoggingRedirectFlagSet()
            ? hud_display::AshTracingRequest::Status::kCompleted
            : hud_display::AshTracingRequest::Status::kPendingMount);

    // Start another trace and continue into user session.
    ToggleAshHUDTracing();
    ++expected_io_registry_size;

    // Wait for tracing to start.
    WaiFortLastTracingRequestStatus(
        hud_display::AshTracingRequest::Status::kStarted);

    // Check that trace was started using correct destination.
    EXPECT_EQ(test_tracing_session_registry->sessions().size(), 2u);
    EXPECT_EQ(test_ash_trace_destination_io_registry->sessions().size(),
              expected_io_registry_size);
    LOG(INFO) << "########### Verifying AshTraceDestination for the trace "
                 "started before login.";
    VerifyTraceDestination(
        test_ash_trace_destination_io_registry->sessions().back().get(),
        /*user_logged_in=*/false);
  }

  // Turn HUD off.
  EXPECT_EQ(IsAshDebugFlagSet(), trigger_hud());
  EXPECT_FALSE(ShellTestApi().IsHUDShown());

  EXPECT_FALSE(user_manager::UserManager::Get()->IsUserLoggedIn());
  Login();
  ASSERT_TRUE(user_manager::UserManager::Get()->IsUserLoggedIn());
  LOG(INFO) << "########### User logged in.";

  EXPECT_FALSE(ShellTestApi().IsHUDShown());

  // Make sure that Ash HUD can be triggered if and only if
  // --ash-debug-shortcuts flag is set.
  EXPECT_EQ(IsAshDebugFlagSet(), trigger_hud());
  EXPECT_EQ(IsAshDebugFlagSet(), ShellTestApi().IsHUDShown());

  {
    LOG(INFO)
        << "########### Verifying AshTraceDestination generation after login.";

    // Calculate trace destination and verify it.
    // Trace destination should be correct even if HUD UI is not enabled, so
    // we verify it separately.
    auto destination = TraceDestinationWaiter().Wait(base::Time::Now());
    VerifyTraceDestination(
        static_cast<TestAshTraceDestinationIO*>(destination->io())->status(),
        /*user_logged_in=*/true);
    ++expected_io_registry_size;
  }

  if (IsAshDebugFlagSet()) {
    // HUD is visible now.
    // Switch HUD to settings view.
    ASSERT_FALSE(GetAshHUDSettingsViewVisible());
    ToggleAshHUDSettingsView();
    ASSERT_TRUE(GetAshHUDSettingsViewVisible());

    // Make sure saved trace is here and another tracing still continues.
    EXPECT_EQ(test_tracing_session_registry->sessions().size(), 2u);
    EXPECT_EQ(hud_display::AshTracingManager::Get()
                  .GetTracingRequestsForTesting()
                  .size(),
              2u);

    if (!IsDisableLoggingRedirectFlagSet()) {
      // We have a pending trace from before the user login. It needs to create
      // another AshTraceDestination to store data to user Downloads directory.
      // Wait for pending trace to finish data copy.
      //
      // Another trace is running, so we wait for all but last tracing sessions
      // to get to kCompleted state.
      WaitForAllButLastTracingRequestsToComplete();
      // Add AshTraceDestination that was created by pending trace.
      ++expected_io_registry_size;
    }
    EXPECT_EQ(test_ash_trace_destination_io_registry->sessions().size(),
              expected_io_registry_size);

    // Make sure tracing continues to the old destination (i.e. when
    // user_logged_in=false).
    LOG(INFO) << "########### Verifying AshTraceDestination for trace started "
                 "before login and still running into user session.";
    VerifyTraceDestination(static_cast<TestAshTraceDestinationIO*>(
                               hud_display::AshTracingManager::Get()
                                   .GetTracingRequestsForTesting()
                                   .back()
                                   ->GetTraceDestinationForTesting()
                                   ->io())
                               ->status(),
                           /*user_logged_in=*/false);

    // Stop tracing.
    ToggleAshHUDTracing();

    // Wait for tracing to finish.
    WaiFortLastTracingRequestStatus(
        hud_display::AshTracingRequest::Status::kCompleted);

    // We still have two records of ash traces.
    EXPECT_EQ(test_tracing_session_registry->sessions().size(), 2u);

    // Tracing session is completely stopped.
    EXPECT_EQ(test_tracing_session_registry->sessions().back()->status,
              TestTracingSessionRegistry::SessionStatus::Status::kDestroyed);

    // - If logging redirect was disabled, trace destination should always be
    // real file, so the original destination should be final.
    // - If it was enabled, after log in, destination should change (new
    // destination should be created) after trace was stopped, and trace should
    // be copied to the user Downloads folder.
    const unsigned expected_destinations_number =
        IsDisableLoggingRedirectFlagSet() ? expected_io_registry_size
                                          : expected_io_registry_size + 1;
    EXPECT_EQ(test_ash_trace_destination_io_registry->sessions().size(),
              expected_destinations_number);

    LOG(INFO) << "########### Verifying AshTraceDestination for the trace "
                 "started before login and finished in user session.";
    // Make sure final trace destination is correct.
    VerifyTraceDestination(static_cast<TestAshTraceDestinationIO*>(
                               hud_display::AshTracingManager::Get()
                                   .GetTracingRequestsForTesting()
                                   .back()
                                   ->GetTraceDestinationForTesting()
                                   ->io())
                               ->status(),
                           /*user_logged_in=*/true);
  }

  // Turn HUD off.
  EXPECT_EQ(IsAshDebugFlagSet(), trigger_hud());
  EXPECT_FALSE(ShellTestApi().IsHUDShown());
}

INSTANTIATE_TEST_SUITE_P(All,
                         AshHUDLoginTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace ash
