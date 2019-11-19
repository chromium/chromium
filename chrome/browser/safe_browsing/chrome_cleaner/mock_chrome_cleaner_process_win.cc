// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/mock_chrome_cleaner_process_win.h"

#include <windows.h>

#include <stdlib.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/chrome_cleaner/public/proto/chrome_prompt.pb.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace safe_browsing {

namespace {

using CrashPoint = MockChromeCleanerProcess::CrashPoint;
using ItemsReporting = MockChromeCleanerProcess::ItemsReporting;
using PromptUserResponse = chrome_cleaner::PromptUserResponse;
using UwsFoundStatus = MockChromeCleanerProcess::UwsFoundStatus;

constexpr char kCrashPointSwitch[] = "mock-crash-point";
constexpr char kUwsFoundSwitch[] = "mock-uws-found";
constexpr char kRebootRequiredSwitch[] = "mock-reboot-required";
constexpr char kRegistryKeysReportingSwitch[] = "registry-keys-reporting";
constexpr char kExtensionsReportingSwitch[] = "extensions-reporting";
constexpr char kExpectedUserResponseSwitch[] = "mock-expected-user-response";

// MockCleanerResults

class MockCleanerResults {
 public:
  MockCleanerResults(const MockChromeCleanerProcess::Options& options,
                     const base::CommandLine& command_line)
      : options_(options) {
    uint32_t handle_value;
    if (base::StringToUint(command_line.GetSwitchValueNative(
                               chrome_cleaner::kChromeReadHandleSwitch),
                           &handle_value)) {
      read_handle_.Set(base::win::Uint32ToHandle(handle_value));
    }
    if (base::StringToUint(command_line.GetSwitchValueNative(
                               chrome_cleaner::kChromeWriteHandleSwitch),
                           &handle_value)) {
      write_handle_.Set(base::win::Uint32ToHandle(handle_value));
    }
  }

  ~MockCleanerResults() = default;

  void SendScanResults(base::OnceClosure done_closure) {
    base::ScopedClosureRunner call_done_closure(std::move(done_closure));
    if (!read_handle_.IsValid() || !write_handle_.IsValid()) {
      LOG(ERROR) << "IPC pipes were not connected correctly";
      return;
    }

    // Send the protocol version number.
    DWORD bytes_written = 0;
    static const uint8_t kVersion = 1;
    if (!::WriteFile(write_handle_.Get(), &kVersion, sizeof(kVersion),
                     &bytes_written, nullptr)) {
      PLOG(ERROR) << "Error writing protocol version";
      return;
    }

    // Send a PromptUser request.
    chrome_cleaner::ChromePromptRequest request;
    chrome_cleaner::PromptUserRequest* prompt_user =
        request.mutable_prompt_user();
    for (const base::FilePath& file : options_.files_to_delete()) {
      prompt_user->add_files_to_delete(file.AsUTF8Unsafe());
    }
    if (options_.registry_keys().has_value()) {
      for (const base::string16& key : options_.registry_keys().value()) {
        prompt_user->add_registry_keys(base::UTF16ToUTF8(key));
      }
    }
    if (options_.extension_ids().has_value()) {
      for (const base::string16& id : options_.extension_ids().value()) {
        prompt_user->add_extension_ids(base::UTF16ToUTF8(id));
      }
    }
    if (!WriteMessage(request.SerializeAsString()))
      return;

    if (options_.crash_point() == CrashPoint::kAfterRequestSent) {
      ::exit(MockChromeCleanerProcess::kDeliberateCrashExitCode);
    }

    // Wait for the response.
    std::string response_message = ReadResponse();
    if (response_message.empty())
      return;
    chrome_cleaner::PromptUserResponse response;
    if (!response.ParseFromString(response_message)) {
      LOG(ERROR) << "Read invalid PromptUser response: " << response_message;
      return;
    }
    ReceivePromptAcceptance(
        base::BindOnce(&MockCleanerResults::SendCloseConnectionRequest,
                       base::Unretained(this), call_done_closure.Release()),
        response.prompt_acceptance());
  }

  void SendCloseConnectionRequest(base::OnceClosure done_closure) {
    chrome_cleaner::ChromePromptRequest request;
    // Initialize a CloseConnectionRequest
    request.mutable_close_connection();
    WriteMessage(request.SerializeAsString());
    std::move(done_closure).Run();
  }

  PromptUserResponse::PromptAcceptance received_prompt_acceptance() const {
    return received_prompt_acceptance_;
  }

  void ReceivePromptAcceptance(
      base::OnceClosure done_closure,
      PromptUserResponse::PromptAcceptance acceptance) {
    received_prompt_acceptance_ = acceptance;
    if (options_.crash_point() == CrashPoint::kAfterResponseReceived)
      ::exit(MockChromeCleanerProcess::kDeliberateCrashExitCode);
    std::move(done_closure).Run();
  }

 private:
  MockCleanerResults(const MockCleanerResults& other) = delete;
  MockCleanerResults& operator=(const MockCleanerResults& other) = delete;

  bool WriteMessage(const std::string& message) {
    uint32_t message_length = message.size();
    DWORD bytes_written = 0;
    if (!::WriteFile(write_handle_.Get(), &message_length,
                     sizeof(message_length), &bytes_written, nullptr)) {
      PLOG(ERROR) << "Error writing message length";
      return false;
    }
    if (!::WriteFile(write_handle_.Get(), message.c_str(), message_length,
                     &bytes_written, nullptr)) {
      PLOG(ERROR) << "Error writing message";
      return false;
    }
    return true;
  }

  std::string ReadResponse() {
    uint32_t response_length = 0;
    DWORD bytes_read = 0;
    // Include space for the null terminator in the WriteInto call.
    if (!::ReadFile(read_handle_.Get(), &response_length,
                    sizeof(response_length), &bytes_read, nullptr)) {
      PLOG(ERROR) << "Error reading response length";
      return std::string();
    }
    std::string response_message;
    if (!::ReadFile(read_handle_.Get(),
                    base::WriteInto(&response_message, response_length + 1),
                    response_length, &bytes_read, nullptr)) {
      PLOG(ERROR) << "Error reading response message";
      return std::string();
    }
    return response_message;
  }

  MockChromeCleanerProcess::Options options_;
  PromptUserResponse::PromptAcceptance received_prompt_acceptance_ =
      PromptUserResponse::UNSPECIFIED;

  base::win::ScopedHandle read_handle_;
  base::win::ScopedHandle write_handle_;
};

scoped_refptr<extensions::Extension> CreateExtension(const base::string16& name,
                                                     const base::string16& id,
                                                     std::string* error) {
  base::DictionaryValue manifest;
  manifest.SetKey("name", base::Value(name));
  manifest.SetKey("version", base::Value("0"));
  manifest.SetKey("manifest_version", base::Value(2));

  return extensions::Extension::Create(base::FilePath(),
                                       extensions::Manifest::INTERNAL, manifest,
                                       0, base::UTF16ToUTF8(id), error);
}

}  // namespace

const base::char16 MockChromeCleanerProcess::kInstalledExtensionId1[] =
    L"aaaabbbbccccddddeeeeffffgggghhhh";
const base::char16 MockChromeCleanerProcess::kInstalledExtensionName1[] =
    L"Some Extension";
const base::char16 MockChromeCleanerProcess::kInstalledExtensionId2[] =
    L"ababababcdcdcdcdefefefefghghghgh";
const base::char16 MockChromeCleanerProcess::kInstalledExtensionName2[] =
    L"Another Extension";
const base::char16 MockChromeCleanerProcess::kUnknownExtensionId[] =
    L"unexpectedextensionidabcdefghijk";

// static
void MockChromeCleanerProcess::AddMockExtensionsToProfile(Profile* profile) {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  scoped_refptr<extensions::Extension> extension;
  std::string error;

  extension =
      CreateExtension(MockChromeCleanerProcess::kInstalledExtensionName1,
                      MockChromeCleanerProcess::kInstalledExtensionId1, &error);
  if (extension && error.empty()) {
    extension_registry->AddEnabled(extension);
  } else {
    LOG(ERROR) << "Error creating mock extension: " << error;
  }

  extension =
      CreateExtension(MockChromeCleanerProcess::kInstalledExtensionName2,
                      MockChromeCleanerProcess::kInstalledExtensionId2, &error);
  if (extension && error.empty()) {
    extension_registry->AddEnabled(extension);
  } else {
    LOG(ERROR) << "Error creating mock extension: " << error;
  }
}

// static
bool MockChromeCleanerProcess::Options::FromCommandLine(
    const base::CommandLine& command_line,
    Options* options) {
  int registry_keys_reporting_int = -1;
  if (!base::StringToInt(
          command_line.GetSwitchValueASCII(kRegistryKeysReportingSwitch),
          &registry_keys_reporting_int) ||
      registry_keys_reporting_int < 0 ||
      registry_keys_reporting_int >=
          static_cast<int>(ItemsReporting::kNumItemsReporting)) {
    return false;
  }

  int extensions_reporting_int = -1;
  if (!base::StringToInt(
          command_line.GetSwitchValueASCII(kExtensionsReportingSwitch),
          &extensions_reporting_int) ||
      extensions_reporting_int < 0 ||
      extensions_reporting_int >=
          static_cast<int>(ItemsReporting::kNumItemsReporting)) {
    return false;
  }

  options->SetReportedResults(
      command_line.HasSwitch(kUwsFoundSwitch),
      static_cast<ItemsReporting>(registry_keys_reporting_int),
      static_cast<ItemsReporting>(extensions_reporting_int));
  options->set_reboot_required(command_line.HasSwitch(kRebootRequiredSwitch));

  if (command_line.HasSwitch(kCrashPointSwitch)) {
    int crash_point_int = 0;
    if (base::StringToInt(command_line.GetSwitchValueASCII(kCrashPointSwitch),
                          &crash_point_int) &&
        crash_point_int >= 0 &&
        crash_point_int < static_cast<int>(CrashPoint::kNumCrashPoints)) {
      options->set_crash_point(static_cast<CrashPoint>(crash_point_int));
    } else {
      return false;
    }
  }

  if (command_line.HasSwitch(kExpectedUserResponseSwitch)) {
    static const std::vector<PromptUserResponse::PromptAcceptance>
        kValidPromptAcceptanceList{
            PromptUserResponse::UNSPECIFIED,
            PromptUserResponse::ACCEPTED_WITH_LOGS,
            PromptUserResponse::ACCEPTED_WITHOUT_LOGS,
            PromptUserResponse::DENIED,
        };

    int expected_response_int = 0;
    if (!base::StringToInt(
            command_line.GetSwitchValueASCII(kExpectedUserResponseSwitch),
            &expected_response_int)) {
      return false;
    }

    const PromptUserResponse::PromptAcceptance expected_response =
        static_cast<PromptUserResponse::PromptAcceptance>(
            expected_response_int);
    if (!base::Contains(kValidPromptAcceptanceList, expected_response)) {
      return false;
    }

    options->set_expected_user_response(expected_response);
  }

  return true;
}

MockChromeCleanerProcess::Options::Options() = default;

MockChromeCleanerProcess::Options::Options(const Options& other)
    : files_to_delete_(other.files_to_delete_),
      registry_keys_(other.registry_keys_),
      extension_ids_(other.extension_ids_),
      reboot_required_(other.reboot_required_),
      crash_point_(other.crash_point_),
      registry_keys_reporting_(other.registry_keys_reporting_),
      extensions_reporting_(other.extensions_reporting_),
      expected_user_response_(other.expected_user_response_) {}

MockChromeCleanerProcess::Options& MockChromeCleanerProcess::Options::operator=(
    const Options& other) {
  files_to_delete_ = other.files_to_delete_;
  registry_keys_ = other.registry_keys_;
  extension_ids_ = other.extension_ids_;
  reboot_required_ = other.reboot_required_;
  crash_point_ = other.crash_point_;
  registry_keys_reporting_ = other.registry_keys_reporting_;
  extensions_reporting_ = other.extensions_reporting_;
  expected_user_response_ = other.expected_user_response_;
  return *this;
}

MockChromeCleanerProcess::Options::~Options() {}

void MockChromeCleanerProcess::Options::AddSwitchesToCommandLine(
    base::CommandLine* command_line) const {
  if (!files_to_delete_.empty())
    command_line->AppendSwitch(kUwsFoundSwitch);

  if (reboot_required())
    command_line->AppendSwitch(kRebootRequiredSwitch);

  if (crash_point() != CrashPoint::kNone) {
    command_line->AppendSwitchASCII(
        kCrashPointSwitch,
        base::NumberToString(static_cast<int>(crash_point())));
  }

  command_line->AppendSwitchASCII(
      kRegistryKeysReportingSwitch,
      base::NumberToString(static_cast<int>(registry_keys_reporting())));
  command_line->AppendSwitchASCII(
      kExtensionsReportingSwitch,
      base::NumberToString(static_cast<int>(extensions_reporting())));

  if (expected_user_response() != PromptUserResponse::UNSPECIFIED) {
    command_line->AppendSwitchASCII(
        kExpectedUserResponseSwitch,
        base::NumberToString(static_cast<int>(expected_user_response())));
  }
}

void MockChromeCleanerProcess::Options::SetReportedResults(
    bool has_files_to_remove,
    ItemsReporting registry_keys_reporting,
    ItemsReporting extensions_reporting) {
  if (!has_files_to_remove)
    files_to_delete_.clear();
  if (has_files_to_remove) {
    files_to_delete_.push_back(
        base::FilePath(FILE_PATH_LITERAL("/path/to/file1.exe")));
    files_to_delete_.push_back(
        base::FilePath(FILE_PATH_LITERAL("/path/to/other/file2.exe")));
    files_to_delete_.push_back(
        base::FilePath(FILE_PATH_LITERAL("/path/to/some file.dll")));
  }

  registry_keys_reporting_ = registry_keys_reporting;
  switch (registry_keys_reporting) {
    case ItemsReporting::kUnsupported:
      // Defined as an optional object in which a registry keys vector is not
      // present.
      registry_keys_ = base::Optional<std::vector<base::string16>>();
      break;

    case ItemsReporting::kNotReported:
      // Defined as an optional object in which an empty registry keys vector is
      // present.
      registry_keys_ =
          base::Optional<std::vector<base::string16>>(base::in_place);
      break;

    case ItemsReporting::kReported:
      // Defined as an optional object in which a non-empty registry keys vector
      // is present.
      registry_keys_ = base::Optional<std::vector<base::string16>>({
          L"HKCU:32\\Software\\Some\\Unwanted Software",
          L"HKCU:32\\Software\\Another\\Unwanted Software",
      });
      break;

    default:
      NOTREACHED();
  }

  extensions_reporting_ = extensions_reporting;
  switch (extensions_reporting) {
    case ItemsReporting::kUnsupported:
      // Defined as an optional object in which an extensions vector is not
      // present.
      extension_ids_ = base::Optional<std::vector<base::string16>>();
      expected_extension_names_ = base::Optional<std::vector<base::string16>>();
      break;

    case ItemsReporting::kNotReported:
      // Defined as an optional object in which an empty extensions vector is
      // present.
      extension_ids_ =
          base::Optional<std::vector<base::string16>>(base::in_place);
      expected_extension_names_ =
          base::Optional<std::vector<base::string16>>(base::in_place);
      break;

    case ItemsReporting::kReported:
      // Defined as an optional object in which a non-empty extensions vector is
      // present.
      extension_ids_ = base::Optional<std::vector<base::string16>>({
          kInstalledExtensionId1, kInstalledExtensionId2, kUnknownExtensionId,
      });
// Scanner results only fetches extension names on Windows Chrome build.
#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
      expected_extension_names_ = base::Optional<std::vector<base::string16>>({
          kInstalledExtensionName1, kInstalledExtensionName2,
          l10n_util::GetStringFUTF16(
              IDS_SETTINGS_RESET_CLEANUP_DETAILS_EXTENSION_UNKNOWN,
              kUnknownExtensionId),
      });
#else
      expected_extension_names_ =
          base::Optional<std::vector<base::string16>>(base::in_place);
#endif
      break;

    default:
      NOTREACHED();
  }
}

int MockChromeCleanerProcess::Options::ExpectedExitCode(
    PromptUserResponse::PromptAcceptance received_prompt_acceptance) const {
  if (crash_point() != CrashPoint::kNone)
    return kDeliberateCrashExitCode;

  if (files_to_delete_.empty())
    return kNothingFoundExitCode;

  if (received_prompt_acceptance == PromptUserResponse::ACCEPTED_WITH_LOGS ||
      received_prompt_acceptance == PromptUserResponse::ACCEPTED_WITHOUT_LOGS) {
    return reboot_required() ? kRebootRequiredExitCode
                             : kRebootNotRequiredExitCode;
  }

  return kDeclinedExitCode;
}

MockChromeCleanerProcess::MockChromeCleanerProcess() = default;

MockChromeCleanerProcess::~MockChromeCleanerProcess() = default;

bool MockChromeCleanerProcess::InitWithCommandLine(
    const base::CommandLine& command_line) {
  command_line_ = std::make_unique<base::CommandLine>(command_line);
  if (!Options::FromCommandLine(command_line, &options_))
    return false;
  return true;
}

int MockChromeCleanerProcess::Run() {
  // We use EXPECT_*() macros to get good log lines, but since this code is run
  // in a separate process, failing a check in an EXPECT_*() macro will not fail
  // the test. Therefore, we use ::testing::Test::HasFailure() to detect
  // EXPECT_*() failures and return an error code that indicates that the test
  // should fail.
  if (options_.crash_point() == CrashPoint::kOnStartup)
    exit(kDeliberateCrashExitCode);

  base::Thread::Options thread_options(base::MessagePumpType::IO, 0);
  base::Thread io_thread("IPCThread");
  EXPECT_TRUE(io_thread.StartWithOptions(thread_options));
  if (::testing::Test::HasFailure())
    return kInternalTestFailureExitCode;

  MockCleanerResults mock_results(options_, *command_line_);

  if (options_.crash_point() == CrashPoint::kAfterConnection)
    exit(kDeliberateCrashExitCode);

  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;
  // After the response from the parent process is received, this will post a
  // task to unblock the child process's main thread.
  auto quit_closure = base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> main_runner,
         base::Closure quit_closure) {
        main_runner->PostTask(FROM_HERE, std::move(quit_closure));
      },
      base::SequencedTaskRunnerHandle::Get(),
      base::Passed(run_loop.QuitClosure()));

  io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockCleanerResults::SendScanResults,
                                base::Unretained(&mock_results),
                                base::Passed(&quit_closure)));

  run_loop.Run();

  EXPECT_NE(mock_results.received_prompt_acceptance(),
            PromptUserResponse::UNSPECIFIED);
  EXPECT_EQ(mock_results.received_prompt_acceptance(),
            options_.expected_user_response());
  if (::testing::Test::HasFailure())
    return kInternalTestFailureExitCode;
  return options_.ExpectedExitCode(mock_results.received_prompt_acceptance());
}

// Keep the printable names of these enums short since they're used in tests
// with very long parameter lists.

std::ostream& operator<<(std::ostream& out, CrashPoint crash_point) {
  return out << "CrPt" << static_cast<int>(crash_point);
}

std::ostream& operator<<(std::ostream& out, UwsFoundStatus status) {
  return out << "UwS" << static_cast<int>(status);
}

std::ostream& operator<<(
    std::ostream& out,
    MockChromeCleanerProcess::ExtensionCleaningFeatureStatus status) {
  return out << "Ext" << static_cast<int>(status);
}

std::ostream& operator<<(std::ostream& out, ItemsReporting items_reporting) {
  return out << "Items" << static_cast<int>(items_reporting);
}

}  // namespace safe_browsing
