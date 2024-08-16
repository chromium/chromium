// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/session_log_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/holding_space/mock_holding_space_client.h"
#include "ash/system/diagnostics/diagnostics_browser_delegate.h"
#include "ash/system/diagnostics/diagnostics_log_controller.h"
#include "ash/system/diagnostics/log_test_helpers.h"
#include "ash/system/diagnostics/networking_log.h"
#include "ash/system/diagnostics/routine_log.h"
#include "ash/system/diagnostics/telemetry_log.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/diagnostics_ui/mojom/system_data_provider.mojom.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/values.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

namespace ash::diagnostics {
namespace {

constexpr char kHandlerFunctionName[] = "handlerFunctionName";

mojom::SystemInfoPtr CreateSystemInfoPtr(const std::string& board_name,
                                         const std::string& marketing_name,
                                         const std::string& cpu_model,
                                         uint32_t total_memory_kib,
                                         uint16_t cpu_threads_count,
                                         uint32_t cpu_max_clock_speed_khz,
                                         bool has_battery,
                                         const std::string& milestone_version,
                                         const std::string& full_version) {
  auto version_info = mojom::VersionInfo::New(milestone_version, full_version);
  auto device_capabilities = mojom::DeviceCapabilities::New(has_battery);

  auto system_info = mojom::SystemInfo::New(
      board_name, marketing_name, cpu_model, total_memory_kib,
      cpu_threads_count, cpu_max_clock_speed_khz, std::move(version_info),
      std::move(device_capabilities));
  return system_info;
}

std::vector<std::string> GetCombinedLogContents(
    const base::FilePath& log_path) {
  std::string contents;
  base::ReadFileToString(log_path, &contents);
  return GetLogLines(contents);
}

// A fake DiagnosticsBrowserDelegate.
class FakeDiagnosticsBrowserDelegate : public DiagnosticsBrowserDelegate {
 public:
  FakeDiagnosticsBrowserDelegate() = default;
  ~FakeDiagnosticsBrowserDelegate() override = default;

  base::FilePath GetActiveUserProfileDir() override {
    base::ScopedTempDir tmp_dir;
    EXPECT_TRUE(tmp_dir.CreateUniqueTempDir());
    return tmp_dir.GetPath();
  }
};

// A fake ui::SelectFileDialog.
class TestSelectFileDialog : public ui::SelectFileDialog {
 public:
  TestSelectFileDialog(Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy,
                       base::FilePath selected_path)
      : ui::SelectFileDialog(listener, std::move(policy)),
        selected_path_(selected_path) {}

  TestSelectFileDialog(const TestSelectFileDialog&) = delete;
  TestSelectFileDialog& operator=(const TestSelectFileDialog&) = delete;

 protected:
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override {
    if (selected_path_.empty()) {
      listener_->FileSelectionCanceled();
      return;
    }

    listener_->FileSelected(ui::SelectedFileInfo(selected_path_), /*index=*/0);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return true;
  }
  void ListenerDestroyed() override { listener_ = nullptr; }
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~TestSelectFileDialog() override = default;

  // The simulatd file path selected by the user.
  base::FilePath selected_path_;
};

// A factory associated with the artificial file picker.
class TestSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  explicit TestSelectFileDialogFactory(base::FilePath selected_path)
      : selected_path_(selected_path) {}

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new TestSelectFileDialog(listener, std::move(policy),
                                    selected_path_);
  }

  TestSelectFileDialogFactory(const TestSelectFileDialogFactory&) = delete;
  TestSelectFileDialogFactory& operator=(const TestSelectFileDialogFactory&) =
      delete;

 private:
  // The simulated file path selected by the user.
  base::FilePath selected_path_;
};

// Test class using NoSessionAshTestBase to ensure shell is available for
// tests requiring DiagnosticsLogController singleton.
class SessionLogHandlerTest : public NoSessionAshTestBase {
 public:
  SessionLogHandlerTest()
      : NoSessionAshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        task_runner_(new base::TestSimpleTaskRunner()) {}
  ~SessionLogHandlerTest() override = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    // Setup to ensure ash::Shell can configure for tests.
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    NoSessionAshTestBase::SetUp();
    DiagnosticsLogController::Initialize(
        std::make_unique<FakeDiagnosticsBrowserDelegate>());
    session_log_handler_ = std::make_unique<diagnostics::SessionLogHandler>(
        base::BindRepeating(
            [](content::WebContents*) -> std::unique_ptr<ui::SelectFilePolicy> {
              return nullptr;
            }),
        /*telemetry_log*/ nullptr, /*routine_log*/ nullptr,
        /*networking_log*/ nullptr, &holding_space_client_);
    session_log_handler_->SetWebUIForTest(&web_ui_);
    session_log_handler_->RegisterMessages();
    session_log_handler_->SetTaskRunnerForTesting(task_runner_);

    // This test suite does not check `HoldingSpaceItem` ids, so there is no
    // need to save the mock id.
    ON_CALL(holding_space_client(), AddItemOfType)
        .WillByDefault(testing::ReturnRef(base::EmptyString()));

    // Call handler to enable Javascript.
    base::Value::List args;
    web_ui_.HandleReceivedMessage("initialize", args);
  }

  void TearDown() override {
    task_runner_.reset();
    task_environment()->RunUntilIdle();
    ui::SelectFileDialog::SetFactory(nullptr);

    NoSessionAshTestBase::TearDown();
  }

  void RunTasks() { task_runner_->RunPendingTasks(); }

  const content::TestWebUI::CallData& CallDataAtIndex(size_t index) {
    return *web_ui_.call_data()[index];
  }

  testing::NiceMock<ash::MockHoldingSpaceClient>& holding_space_client() {
    return holding_space_client_;
  }

 protected:
  // Task runner for tasks posted by save session log handler.
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  content::TestWebUI web_ui_;
  std::unique_ptr<SessionLogHandler> session_log_handler_;
  base::ScopedTempDir temp_dir_;
  testing::NiceMock<ash::MockHoldingSpaceClient> holding_space_client_;
};

}  // namespace

TEST_F(SessionLogHandlerTest, SaveSessionLog) {
  // Run until idle to finish necessary setup.
  task_environment()->RunUntilIdle();

  base::RunLoop run_loop;
  // Populate routine log
  DiagnosticsLogController::Get()->GetRoutineLog().LogRoutineStarted(
      mojom::RoutineType::kCpuStress);
  task_environment()->RunUntilIdle();

  // Populate telemetry log
  const std::string expected_board_name = "board_name";
  const std::string expected_marketing_name = "marketing_name";
  const std::string expected_cpu_model = "cpu_model";
  const uint32_t expected_total_memory_kib = 1234;
  const uint16_t expected_cpu_threads_count = 5678;
  const uint32_t expected_cpu_max_clock_speed_khz = 91011;
  const bool expected_has_battery = true;
  const std::string expected_milestone_version = "M99";
  const std::string expected_full_version = "M99.1234.5.6";
  mojom::SystemInfoPtr test_info = CreateSystemInfoPtr(
      expected_board_name, expected_marketing_name, expected_cpu_model,
      expected_total_memory_kib, expected_cpu_threads_count,
      expected_cpu_max_clock_speed_khz, expected_has_battery,
      expected_milestone_version, expected_full_version);

  DiagnosticsLogController::Get()->GetTelemetryLog().UpdateSystemInfo(
      std::move(test_info));

  // Select file
  base::FilePath log_path = temp_dir_.GetPath().AppendASCII("test_path");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<TestSelectFileDialogFactory>(log_path));
  base::Value::List args;
  args.Append(kHandlerFunctionName);
  session_log_handler_->SetLogCreatedClosureForTest(run_loop.QuitClosure());
  web_ui_.HandleReceivedMessage("saveSessionLog", args);
  run_loop.RunUntilIdle();
  const std::string expected_system_log_header = "=== System ===";
  const std::string expected_system_info_section_name = "--- System Info ---";
  const std::string expected_snapshot_time_prefix = "Snapshot Time: ";
  RunTasks();
  const std::vector<std::string> log_lines = GetCombinedLogContents(log_path);
  ASSERT_EQ(18u, log_lines.size());
  EXPECT_EQ(expected_system_log_header, log_lines[0]);
  EXPECT_EQ(expected_system_info_section_name, log_lines[1]);
  EXPECT_GT(log_lines[2].size(), expected_snapshot_time_prefix.size());
  EXPECT_TRUE(base::StartsWith(log_lines[2], expected_snapshot_time_prefix));
  EXPECT_EQ("Board Name: " + expected_board_name, log_lines[3]);
  EXPECT_EQ("Marketing Name: " + expected_marketing_name, log_lines[4]);
  EXPECT_EQ("CpuModel Name: " + expected_cpu_model, log_lines[5]);
  EXPECT_EQ(
      "Total Memory (kib): " + base::NumberToString(expected_total_memory_kib),
      log_lines[6]);
  EXPECT_EQ(
      "Thread Count:  " + base::NumberToString(expected_cpu_threads_count),
      log_lines[7]);
  EXPECT_EQ("Cpu Max Clock Speed (kHz):  " +
                base::NumberToString(expected_cpu_max_clock_speed_khz),
            log_lines[8]);
  EXPECT_EQ("Version: " + expected_full_version, log_lines[9]);
  EXPECT_EQ("Has Battery: true", log_lines[10]);

  const std::string expected_routine_log_header = "--- Test Routines ---";
  EXPECT_EQ(expected_routine_log_header, log_lines[11]);

  const std::vector<std::string> first_routine_log_line_contents =
      GetLogLineContents(log_lines[12]);
  ASSERT_EQ(3u, first_routine_log_line_contents.size());
  // first_routine_log_line_contents[0] is ignored because it's a timestamp.
  EXPECT_EQ("CpuStress", first_routine_log_line_contents[1]);
  EXPECT_EQ("Started", first_routine_log_line_contents[2]);

  // Networking log contents.
  EXPECT_EQ("=== Networking ===", log_lines[13]);
  EXPECT_EQ("--- Network Info ---", log_lines[14]);
  EXPECT_EQ("--- Test Routines ---", log_lines[15]);
  EXPECT_EQ("No routines of this type were run in the session.", log_lines[16]);
  EXPECT_EQ("--- Network Events ---", log_lines[17]);
}

// Validates behavior when log controller is used to generate session log.
TEST_F(SessionLogHandlerTest, SaveHeaderOnlySessionLog) {
  base::RunLoop run_loop;

  // Simulate select file
  base::FilePath log_path = temp_dir_.GetPath().AppendASCII("test_path");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<TestSelectFileDialogFactory>(log_path));
  base::Value::List args;
  args.Append(kHandlerFunctionName);
  session_log_handler_->SetLogCreatedClosureForTest(run_loop.QuitClosure());
  web_ui_.HandleReceivedMessage("saveSessionLog", args);
  RunTasks();

  const std::vector<std::string> log_lines = GetCombinedLogContents(log_path);
  ASSERT_EQ(8u, log_lines.size());

  // Empty system data log.
  const std::string expected_system_header = "=== System ===";
  EXPECT_EQ(expected_system_header, log_lines[0]);
  const std::string expected_routine_header = "--- Test Routines ---";
  EXPECT_EQ(expected_routine_header, log_lines[1]);
  const std::string expected_no_routine_msg =
      "No routines of this type were run in the session.";
  EXPECT_EQ(expected_no_routine_msg, log_lines[2]);

  // Empty network data log.
  const std::string expected_network_header = "=== Networking ===";
  EXPECT_EQ(expected_network_header, log_lines[3]);
  const std::string expected_network_info_header = "--- Network Info ---";
  EXPECT_EQ(expected_network_info_header, log_lines[4]);
  EXPECT_EQ(expected_routine_header, log_lines[5]);
  EXPECT_EQ(expected_no_routine_msg, log_lines[6]);
  const std::string expected_network_events_header = "--- Network Events ---";
  EXPECT_EQ(expected_network_events_header, log_lines[7]);
}

// Validates that invoking the saveSessionLog Web UI event opens the
// select dialog. Choosing a directory should return that the operation
// was successful.
TEST_F(SessionLogHandlerTest, SelectDirectory) {
  base::FilePath log_path = temp_dir_.GetPath().AppendASCII("test_path");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<TestSelectFileDialogFactory>(log_path));

  const size_t call_data_count_before_call = web_ui_.call_data().size();
  base::Value::List args;
  args.Append(kHandlerFunctionName);
  base::RunLoop run_loop;
  session_log_handler_->SetLogCreatedClosureForTest(run_loop.QuitClosure());
  web_ui_.HandleReceivedMessage("saveSessionLog", args);
  RunTasks();
  run_loop.RunUntilIdle();

  EXPECT_EQ(call_data_count_before_call + 1u, web_ui_.call_data().size());
  const content::TestWebUI::CallData& call_data =
      CallDataAtIndex(call_data_count_before_call);
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
  EXPECT_TRUE(/*success=*/call_data.arg2()->GetBool());
}

TEST_F(SessionLogHandlerTest, CancelDialog) {
  // A dialog returning an empty file path simulates the user closing the
  // dialog without selecting a path.
  ui::SelectFileDialog::SetFactory(
      std::make_unique<TestSelectFileDialogFactory>(base::FilePath()));

  const size_t call_data_count_before_call = web_ui_.call_data().size();
  base::Value::List args;
  args.Append(kHandlerFunctionName);
  web_ui_.HandleReceivedMessage("saveSessionLog", args);
  RunTasks();

  EXPECT_EQ(call_data_count_before_call + 1u, web_ui_.call_data().size());
  const content::TestWebUI::CallData& call_data =
      CallDataAtIndex(call_data_count_before_call);
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
  EXPECT_FALSE(/*success=*/call_data.arg2()->GetBool());
}

// Validates that saving a session log results in the session log file being
// added to the holding space.
TEST_F(SessionLogHandlerTest, AddToHoldingSpace) {
  base::FilePath log_path = temp_dir_.GetPath().AppendASCII("test_path");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<TestSelectFileDialogFactory>(log_path));
  base::Value::List args;
  args.Append(kHandlerFunctionName);

  EXPECT_CALL(holding_space_client(),
              AddItemOfType(HoldingSpaceItem::Type::kDiagnosticsLog,
                            testing::Eq(log_path)));

  base::RunLoop run_loop;
  session_log_handler_->SetLogCreatedClosureForTest(run_loop.QuitClosure());
  web_ui_.HandleReceivedMessage("saveSessionLog", args);
  RunTasks();
  run_loop.RunUntilIdle();
}

// Validates that the lifecycle clean up tasks are completed if the select file
// dialog is open when session_log_handler is destroyed.
TEST_F(SessionLogHandlerTest, CleanUpDialogOnDeconstruct) {
  base::FilePath log_path = temp_dir_.GetPath().AppendASCII("test_path");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<TestSelectFileDialogFactory>(log_path));
  base::Value::List args;
  args.Append(kHandlerFunctionName);
  base::RunLoop run_loop;

  session_log_handler_->SetLogCreatedClosureForTest(run_loop.QuitClosure());
  web_ui_.HandleReceivedMessage("saveSessionLog", args);
  EXPECT_NO_FATAL_FAILURE(session_log_handler_.reset());
  EXPECT_NO_FATAL_FAILURE(task_runner_.reset());
  EXPECT_NO_FATAL_FAILURE(run_loop.RunUntilIdle());
}

// Validates CreateSessionLog task does not trigger a Use-After-Free error
// when SessionLogHandler is destroyed before task is run. See crbug/1328708.
TEST_F(SessionLogHandlerTest, NoUseAfterFree) {
  base::FilePath log_path = temp_dir_.GetPath().AppendASCII("test_path");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<TestSelectFileDialogFactory>(log_path));
  base::Value::List args;
  args.Append(kHandlerFunctionName);
  base::RunLoop run_loop;

  session_log_handler_->SetLogCreatedClosureForTest(
      base::BindLambdaForTesting([]() { NOTREACHED(); }));
  EXPECT_EQ(0u, task_runner_->NumPendingTasks());
  web_ui_.HandleReceivedMessage("saveSessionLog", args);
  EXPECT_EQ(1u, task_runner_->NumPendingTasks());
  EXPECT_NO_FATAL_FAILURE(session_log_handler_.reset());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(0u, task_runner_->NumPendingTasks());
  EXPECT_NO_FATAL_FAILURE(task_runner_->RunUntilIdle());
}

}  // namespace ash::diagnostics
