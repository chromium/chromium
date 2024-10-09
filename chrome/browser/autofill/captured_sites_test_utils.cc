// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/captured_sites_test_utils.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/check_deref.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "components/permissions/permission_request_manager.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_channel_factory.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_sync_message.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"

using base::JSONParserOptions;
using base::JSONReader;

namespace {

// The command-line flag to specify the command file flag.
const char kCommandFileFlag[] = "command_file";

// The command line flag to turn on verbose WPR logging.
const char kWebPageReplayVerboseFlag[] = "wpr_verbose";

// The maximum amount of time to wait for Chrome to finish autofilling a form.
const base::TimeDelta kAutofillActionWaitForVisualUpdateTimeout =
    base::Seconds(3);

// The number of tries the TestRecipeReplayer should perform when executing an
// Chrome Autofill action.
// Chrome Autofill can be flaky on some real-world pages. The Captured Site
// Automation Framework will retry an autofill action a couple times before
// concluding that Chrome Autofill does not work.
const int kAutofillActionNumRetries = 5;

// The public key hash for the certificate Web Page Replay (WPR) uses to serve
// HTTPS content.
// The Captured Sites Test Framework relies on WPR to serve captured site
// traffic. If a machine does not have the WPR certificate installed, Chrome
// will detect a server certificate validation failure when WPR serves Chrome
// HTTPS content. In response Chrome will block the WPR HTTPS content.
// The test framework avoids this problem by launching Chrome with the
// ignore-certificate-errors-spki-list flag set to the WPR certificate's
// public key hash. Doing so tells Chrome to ignore server certificate
// validation errors from WPR.
const char kWebPageReplayCertSPKI[] =
    "PoNnQAwghMiLUPg1YNFtvTfGreNT8r9oeLEyzgNCJWc=";

const char kClockNotSetMessage[] =
    "No AutofillClock override set from wpr archive: ";

// Check and return that the caller wants verbose WPR output (off by default).
bool IsVerboseWprLoggingEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line && command_line->HasSwitch(kWebPageReplayVerboseFlag);
}

void PrintDebugInstructions(const base::FilePath& command_file_path) {
  const char msg[] = R"(

*** INTERACTIVE DEBUGGING MODE ***

To proceed, you should create a named pipe:
  $ mkfifo %1$s
and then write commands into it:
  $ echo run     >%1$s  # unpauses execution
  $ echo next 2  >%1$s  # executes the next 2 actions
  $ echo next -1 >%1$s  # executes until the last action
  $ echo skip -3 >%1$s  # jumps back 3 actions
  $ echo skip 4  >%1$s  # skips the next 4 actions
  $ echo where   >%1$s  # prints the current position
  $ echo show -1 >%1$s  # prints last 1 actions
  $ echo show 1  >%1$s  # prints next 1 actions
  $ echo failure >%1$s  # unpauses execution until failure
  $ echo help    >%1$s  # prints this text
)";
  VLOG(1) << base::StringPrintf(msg, command_file_path.AsUTF8Unsafe().c_str());
}

std::optional<autofill::FieldType> StringToFieldType(std::string_view str) {
  static auto map = []() {
    std::map<std::string_view, autofill::FieldType> map;
    for (autofill::FieldType field_type : autofill::kAllFieldTypes) {
      map[autofill::AutofillType(field_type).ToStringView()] = field_type;
    }
    for (autofill::HtmlFieldType html_field_type :
         autofill::kAllHtmlFieldTypes) {
      autofill::AutofillType field_type(html_field_type);
      map[field_type.ToStringView()] = field_type.GetStorableType();
    }
    return map;
  }();
  auto it = map.find(str);
  if (it == map.end()) {
    return std::nullopt;
  }
  return it->second;
}

// Command types to control and debug execution.
// * The |kAbsoluteLimit| and |kRelativeLimit| commands indicate that
//   execution shall not proceed if the next action's position is >= |param|
//   or >= current_index + |param|, respectively.
// * The |kSkipAction| command jumps |param| actions forward or backward.
// * The |kShowAction| command prints the |param| previous (if < 0) or
//   upcoming (if > 0) actions.
// * The |kWhereAmI| command prints the current execution position.
enum class ExecutionCommandType {
  kAbsoluteLimit,
  kRelativeLimit,
  kSkipAction,
  kShowAction,
  kWhereAmI,
  kRunUntilFailure
};

struct ExecutionCommand {
  ExecutionCommandType type = ExecutionCommandType::kAbsoluteLimit;
  int param = std::numeric_limits<int>::max();
};

// Blockingly reads the content of |command_file_path|, parses it into
// ExecutionCommands, and returns the result.
std::vector<ExecutionCommand> ReadExecutionCommands(
    const base::FilePath& command_file_path) {
  std::vector<ExecutionCommand> commands;
  std::string command_lines;
  if (base::ReadFileToString(command_file_path, &command_lines)) {
    for (const auto command :
         base::SplitStringPiece(command_lines, "\n", base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY)) {
      auto GetParamOr = [command](int default_value) {
        size_t space = command.find(' ');
        if (space == std::string_view::npos) {
          return default_value;
        }
        int value;
        if (!base::StringToInt(command.substr(space + 1), &value))
          return default_value;
        return value;
      };

      if (command.starts_with("run")) {
        commands.push_back({ExecutionCommandType::kAbsoluteLimit,
                            std::numeric_limits<int>::max()});
      } else if (command.starts_with("next")) {
        commands.push_back(
            {ExecutionCommandType::kRelativeLimit, GetParamOr(1)});
      } else if (command.starts_with("skip")) {
        commands.push_back({ExecutionCommandType::kSkipAction, GetParamOr(1)});
      } else if (command.starts_with("show")) {
        commands.push_back({ExecutionCommandType::kShowAction, GetParamOr(1)});
      } else if (command.starts_with("where")) {
        commands.push_back({ExecutionCommandType::kWhereAmI});
      } else if (command.starts_with("failure")) {
        commands.push_back({ExecutionCommandType::kRunUntilFailure});
        // Same commands as for "run":
        commands.push_back({ExecutionCommandType::kAbsoluteLimit,
                            std::numeric_limits<int>::max()});
      } else if (command.starts_with("help")) {
        PrintDebugInstructions(command_file_path);
      }
    }
  }
  return commands;
}

struct ExecutionState {
  // The position of the next action to be executed.
  int index = 0;
  // The current bound on the execution.
  int limit = std::numeric_limits<int>::max();
  // The number of actions to be executed.
  int length = 0;
  // Whether to stop at a step if a failure is detected.
  bool pause_on_failure = false;
};

// Blockingly reads the commands from |command_file_path| and executes them.
// Execution primarily means manipulation of the |execution_state|, particularly
// `execution_state.limit`.
ExecutionState ProcessCommands(ExecutionState execution_state,
                               const base::Value::List* action_list,
                               const base::FilePath& command_file_path) {
  while (execution_state.limit <= execution_state.index) {
    for (ExecutionCommand command : ReadExecutionCommands(command_file_path)) {
      switch (command.type) {
        case ExecutionCommandType::kAbsoluteLimit: {
          execution_state.limit = command.param;
          break;
        }
        case ExecutionCommandType::kRelativeLimit: {
          if (command.param >= 0) {
            execution_state.limit += command.param;
          } else {
            execution_state.limit = execution_state.length + command.param;
          }
          break;
        }
        case ExecutionCommandType::kSkipAction: {
          execution_state.index += command.param;
          execution_state.index = std::min(std::max(execution_state.index, 0),
                                           execution_state.length - 1);
          break;
        }
        case ExecutionCommandType::kShowAction: {
          int min_index = execution_state.index + std::min(command.param, 0);
          int max_index = execution_state.index + std::max(command.param, 0);
          min_index = std::max(min_index, 0);
          max_index = std::min(max_index, execution_state.length);
          for (int i = min_index; i < max_index; ++i) {
            VLOG(1) << "Action " << (i - execution_state.index) << ": "
                    << (*action_list)[i].DebugString();
          }
          break;
        }
        case ExecutionCommandType::kWhereAmI: {
          VLOG(1) << "Next action is at position " << execution_state.index
                  << ", limit (excl) is at " << execution_state.limit
                  << ", last (excl) is at " << execution_state.length;
          break;
        }
        case ExecutionCommandType::kRunUntilFailure: {
          VLOG(1) << "Will stop when a failure is found.";
          execution_state.pause_on_failure = true;
          break;
        }
      }
    }
  }
  return execution_state;
}

struct AllowNull {
  inline constexpr AllowNull() = default;
};

std::optional<std::string> FindPopulateString(
    const base::Value::Dict& container,
    std::string_view key_name,
    absl::variant<std::string_view, AllowNull> key_descriptor) {
  const std::string* value = container.FindString(key_name);
  if (!value) {
    if (absl::holds_alternative<std::string_view>(key_descriptor)) {
      ADD_FAILURE() << "Failed to extract '"
                    << absl::get<std::string_view>(key_descriptor)
                    << "' string from container!";
    }
    return std::nullopt;
  }

  return *value;
}

std::optional<std::vector<std::string>> FindPopulateStringVector(
    const base::Value::Dict& container,
    std::string_view key_name,
    absl::variant<std::string_view, AllowNull> key_descriptor) {
  const base::Value::List* list = container.FindList(key_name);
  if (!list) {
    if (absl::holds_alternative<std::string_view>(key_descriptor)) {
      ADD_FAILURE() << "Failed to extract '"
                    << absl::get<std::string_view>(key_descriptor)
                    << "' strings from container!";
    }
    return std::nullopt;
  }

  std::vector<std::string> strings;
  for (const base::Value& item : *list) {
    if (!item.is_string()) {
      if (absl::holds_alternative<std::string_view>(key_descriptor)) {
        ADD_FAILURE() << "Failed to extract element of '"
                      << absl::get<std::string_view>(key_descriptor)
                      << "' vector from container!";
      }
      return std::nullopt;
    }
    strings.push_back(item.GetString());
  }
  return strings;
}

}  // namespace

namespace captured_sites_test_utils {

CapturedSiteParams::CapturedSiteParams() = default;
CapturedSiteParams::~CapturedSiteParams() = default;
CapturedSiteParams::CapturedSiteParams(const CapturedSiteParams& other) =
    default;

std::ostream& operator<<(std::ostream& os, const CapturedSiteParams& param) {
  if (param.scenario_dir.empty())
    return os << "Site: " << param.site_name;
  return os << "Scenario: " << param.scenario_dir
            << ", Site: " << param.site_name;
}

// Iterate through Autofill's Web Page Replay capture file directory to look
// for captures sites and automation recipe files. Return a list of sites for
// which recipe-based testing is available.
std::vector<CapturedSiteParams> GetCapturedSites(
    const base::FilePath& replay_files_dir_path) {
  std::vector<CapturedSiteParams> sites;
  base::FilePath config_file_path =
      replay_files_dir_path.AppendASCII("testcases.json");

  std::string json_text;
  if (!base::ReadFileToString(config_file_path, &json_text)) {
    LOG(WARNING) << "Could not read json file: " << config_file_path;
    return sites;
  }
  // Parse json text content to json value node.
  auto value_with_error = JSONReader::ReadAndReturnValueWithError(
      json_text, JSONParserOptions::JSON_PARSE_RFC);
  if (!value_with_error.has_value()) {
    LOG(WARNING) << "Could not load test config from json file: "
                 << "`testcases.json` because: "
                 << value_with_error.error().message;
    return sites;
  }
  base::Value::Dict root_node = std::move(*value_with_error).TakeDict();
  const base::Value::List* list_node = root_node.FindList("tests");
  if (!list_node) {
    LOG(WARNING) << "No tests found in `testcases.json` config";
    return sites;
  }

  bool also_run_disabled = GTEST_FLAG_GET(also_run_disabled_tests);

  for (auto& item_val : *list_node) {
    if (!item_val.is_dict()) {
      continue;
    }
    const base::Value::Dict& item = item_val.GetDict();
    CapturedSiteParams param;
    param.site_name = CHECK_DEREF(item.FindString("site_name"));

    if (const std::string* scenario_dir = item.FindString("scenario_dir")) {
      param.scenario_dir = *scenario_dir;
    }
    param.is_disabled = item.FindBool("disabled").value_or(false);

    const std::optional<int> bug_number = item.FindInt("bug_number");
    if (bug_number) {
      param.bug_number = bug_number.value();
    }
    if (param.is_disabled && !also_run_disabled)
      continue;

    const std::string* expectation_string = item.FindString("expectation");
    if (expectation_string && *expectation_string == "FAIL") {
      param.expectation = kFail;
    } else {
      param.expectation = kPass;
    }
    // Check that a pair of .test and .wpr files exist - otherwise skip
    base::FilePath file_name = replay_files_dir_path;
    base::FilePath refresh_file_path =
        replay_files_dir_path.AppendASCII("refresh");
    if (!param.scenario_dir.empty()) {
      file_name = file_name.AppendASCII(param.scenario_dir);
      refresh_file_path = refresh_file_path.AppendASCII(param.scenario_dir);
    }
    file_name = file_name.AppendASCII(param.site_name);
    refresh_file_path =
        refresh_file_path.AppendASCII(param.site_name).AddExtensionASCII("wpr");

    base::FilePath capture_file_path = file_name.AddExtensionASCII("wpr");
    if (!base::PathExists(capture_file_path)) {
      LOG(WARNING) << "Test `" << param.site_name
                   << "` had no matching .wpr file";
      continue;
    }
    base::FilePath recipe_file_path = file_name.AddExtensionASCII("test");
    if (!base::PathExists(recipe_file_path)) {
      LOG(WARNING) << "Test `" << param.site_name
                   << "` had no matching .test file";
      continue;
    }
    param.capture_file_path = capture_file_path;
    param.recipe_file_path = recipe_file_path;
    param.refresh_file_path = refresh_file_path;
    sites.push_back(param);
  }
  return sites;
}

std::string FilePathToUTF8(const base::FilePath::StringType& str) {
#if BUILDFLAG(IS_WIN)
  return base::WideToUTF8(str);
#else
  return str;
#endif
}

std::optional<base::FilePath> GetCommandFilePath() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line && command_line->HasSwitch(kCommandFileFlag)) {
    return std::make_optional(
        command_line->GetSwitchValuePath(kCommandFileFlag));
  }
  return std::nullopt;
}

void PrintInstructions(const char* test_file_name) {
  const char msg[] = R"(

*** README ***

A captured-site test replays an action recipe (*.test)
against recorded web traffic (*.wpr).
These files need to be downloaded separately.

Recommended flags are:
  --test-launcher-timeout=1000000
  --ui-test-action-max-timeout=1000000
  --enable-pixel-output-in-tests
  --vmodule=captured_sites_test_utils=1,%s=1:

For interactive debugging, specify a command file:
  --%s=/path/to/file
Commands to step through the test can be written into that file.
Further instructions will be printed then.
)";
  VLOG(1) << base::StringPrintf(msg, test_file_name, kCommandFileFlag);
}

// FrameObserver --------------------------------------------------------------
IFrameWaiter::IFrameWaiter(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      query_type_(URL),
      target_frame_(nullptr) {}

IFrameWaiter::~IFrameWaiter() {}

content::RenderFrameHost* IFrameWaiter::WaitForFrameMatchingName(
    const std::string& name,
    const base::TimeDelta timeout) {
  content::RenderFrameHost* frame = FrameMatchingPredicateOrNullptr(
      web_contents()->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, name));
  if (frame) {
    return frame;
  } else {
    query_type_ = NAME;
    frame_name_ = name;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop_.QuitClosure(), timeout);
    run_loop_.Run();
    return target_frame_;
  }
}

content::RenderFrameHost* IFrameWaiter::WaitForFrameMatchingOrigin(
    const GURL origin,
    const base::TimeDelta timeout) {
  content::RenderFrameHost* frame = FrameMatchingPredicateOrNullptr(
      web_contents()->GetPrimaryPage(),
      base::BindRepeating(&FrameHasOrigin, origin));
  if (frame) {
    return frame;
  } else {
    query_type_ = ORIGIN;
    origin_ = origin;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop_.QuitClosure(), timeout);
    run_loop_.Run();
    return target_frame_;
  }
}

content::RenderFrameHost* IFrameWaiter::WaitForFrameMatchingUrl(
    const GURL url,
    const base::TimeDelta timeout) {
  content::RenderFrameHost* frame = FrameMatchingPredicateOrNullptr(
      web_contents()->GetPrimaryPage(),
      base::BindRepeating(&content::FrameHasSourceUrl, url));
  if (frame) {
    return frame;
  } else {
    query_type_ = URL;
    url_ = url;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop_.QuitClosure(), timeout);
    run_loop_.Run();
    return target_frame_;
  }
}

void IFrameWaiter::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (!run_loop_.running())
    return;
  switch (query_type_) {
    case NAME:
      if (FrameMatchesName(frame_name_, render_frame_host))
        run_loop_.Quit();
      break;
    case ORIGIN:
      if (render_frame_host->GetLastCommittedURL().DeprecatedGetOriginAsURL() ==
          origin_)
        run_loop_.Quit();
      break;
    case URL:
      if (FrameHasSourceUrl(url_, render_frame_host))
        run_loop_.Quit();
      break;
    default:
      break;
  }
}

void IFrameWaiter::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                                 const GURL& validated_url) {
  if (!run_loop_.running())
    return;
  switch (query_type_) {
    case ORIGIN:
      if (validated_url.DeprecatedGetOriginAsURL() == origin_)
        run_loop_.Quit();
      break;
    case URL:
      if (FrameHasSourceUrl(validated_url, render_frame_host))
        run_loop_.Quit();
      break;
    default:
      break;
  }
}

void IFrameWaiter::FrameNameChanged(content::RenderFrameHost* render_frame_host,
                                    const std::string& name) {
  if (!run_loop_.running())
    return;
  switch (query_type_) {
    case NAME:
      if (FrameMatchesName(name, render_frame_host))
        run_loop_.Quit();
      break;
    default:
      break;
  }
}

bool IFrameWaiter::FrameHasOrigin(const GURL& origin,
                                  content::RenderFrameHost* frame) {
  GURL url = frame->GetLastCommittedURL();
  return (url.DeprecatedGetOriginAsURL() == origin.DeprecatedGetOriginAsURL());
}

// WebPageReplayServerWrapper -------------------------------------------------
WebPageReplayServerWrapper::WebPageReplayServerWrapper(
    const bool start_as_replay,
    int host_http_port,
    int host_https_port)
    : host_http_port_(host_http_port),
      host_https_port_(host_https_port),
      start_as_replay_(start_as_replay) {}

WebPageReplayServerWrapper::~WebPageReplayServerWrapper() = default;

bool WebPageReplayServerWrapper::Start(
    const base::FilePath& capture_file_path) {
  std::vector<std::string> args;
  base::FilePath src_dir;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir)) {
    ADD_FAILURE() << "Failed to extract the Chromium source directory!";
    return false;
  }

  args.push_back(base::StringPrintf("--http_port=%d", host_http_port_));
  args.push_back(base::StringPrintf("--https_port=%d", host_https_port_));
  if (start_as_replay_) {
    args.push_back("--serve_response_in_chronological_sequence");
    // Start WPR in quiet mode, removing the extra verbose ServeHTTP
    // interactions that are for the the overwhelming majority unhelpful, but
    // for extra debugging of a test case, include 'wpr_verbose' in command.
    if (!IsVerboseWprLoggingEnabled())
      args.push_back("--quiet_mode");
  }
  args.push_back(base::StringPrintf(
      "--inject_scripts=%s,%s",
      FilePathToUTF8(src_dir.AppendASCII("third_party")
                         .AppendASCII("catapult")
                         .AppendASCII("web_page_replay_go")
                         .AppendASCII("deterministic.js")
                         .value())
          .c_str(),
      FilePathToUTF8(src_dir.AppendASCII("chrome")
                         .AppendASCII("test")
                         .AppendASCII("data")
                         .AppendASCII("web_page_replay_go_helper_scripts")
                         .AppendASCII("automation_helper.js")
                         .value())
          .c_str()));

  // Specify the capture file.
  args.push_back(base::StringPrintf(
      "%s", FilePathToUTF8(capture_file_path.value()).c_str()));
  if (!RunWebPageReplayCmd(args))
    return false;

  // Sleep 5 seconds to wait for the web page replay server to start.
  // TODO(crbug.com/40578543): create a process std stream reader class to use
  // the process output to determine when the server is ready
  base::RunLoop wpr_launch_waiter;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, wpr_launch_waiter.QuitClosure(), base::Seconds(5));
  wpr_launch_waiter.Run();

  if (!web_page_replay_server_.IsValid()) {
    ADD_FAILURE() << "Failed to start the WPR replay server!";
    return false;
  }

  return true;
}

bool WebPageReplayServerWrapper::Stop() {
  if (web_page_replay_server_.IsValid()) {
    bool did_terminate = false;
    if (!start_as_replay_) {
#if BUILDFLAG(IS_POSIX)
      // For Replay sessions, we can terminate the WPR process immediately as
      // we don't Record sessions, we want to try and send a SIGINT to close and
      // write the WPR archive file gracefully. If that fails, we will Terminate
      // via Process::Terminate which will send SIGTERM and then SIGKILL.
      did_terminate = kill(web_page_replay_server_.Handle(), SIGINT) == 0;
      if (!did_terminate) {
        ADD_FAILURE() << "Failed to close a recording WPR server cleanly!";
      }
#else
      ADD_FAILURE()
          << "Clean termination of recrording WPR server is only supported on "
             "OS_POSIX. New archive may not be saved properly.";
#endif
    }
    if (start_as_replay_ || !did_terminate) {
      if (!web_page_replay_server_.Terminate(0, true)) {
        ADD_FAILURE() << "Failed to terminate the WPR replay server!";
        return false;
      }
    }
  }

  // The test server hasn't started, no op.
  return true;
}

bool WebPageReplayServerWrapper::RunWebPageReplayCmdAndWaitForExit(
    const std::vector<std::string>& args,
    const base::TimeDelta& timeout) {
  int exit_code;

  if (RunWebPageReplayCmd(args) && web_page_replay_server_.IsValid() &&
      web_page_replay_server_.WaitForExitWithTimeout(timeout, &exit_code) &&
      exit_code == 0) {
    return true;
  }

  ADD_FAILURE() << "Failed to run WPR command: '" << cmd_name() << "'!";
  return false;
}

bool WebPageReplayServerWrapper::RunWebPageReplayCmd(
    const std::vector<std::string>& args) {
  // Allow the function to block. Otherwise the subsequent call to
  // base::PathExists will fail. base::PathExists must be called from
  // a scope that allows blocking.
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::LaunchOptions options = base::LaunchOptionsForTest();
  base::FilePath exe_dir;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &exe_dir)) {
    ADD_FAILURE() << "Failed to extract the Chromium source directory!";
    return false;
  }

  base::FilePath web_page_replay_binary_dir = exe_dir.AppendASCII("third_party")
                                                  .AppendASCII("catapult")
                                                  .AppendASCII("telemetry")
                                                  .AppendASCII("telemetry")
                                                  .AppendASCII("bin");
  options.current_directory = web_page_replay_binary_dir;

#if BUILDFLAG(IS_WIN)
  base::FilePath wpr_executable_binary =
      base::FilePath(FILE_PATH_LITERAL("win"))
          .AppendASCII("AMD64")
          .AppendASCII("wpr.exe");
#elif BUILDFLAG(IS_MAC)
  base::FilePath wpr_executable_binary =
      base::FilePath(FILE_PATH_LITERAL("mac"))
          .AppendASCII("x86_64")
          .AppendASCII("wpr");
#elif BUILDFLAG(IS_POSIX)
  base::FilePath wpr_executable_binary =
      base::FilePath(FILE_PATH_LITERAL("linux"))
          .AppendASCII("x86_64")
          .AppendASCII("wpr");
#else
#error Platform is not supported.
#endif
  base::CommandLine full_command(
      web_page_replay_binary_dir.Append(wpr_executable_binary));
  full_command.AppendArg(cmd_name());

  // Ask web page replay to use the custom certificate and key files used to
  // make the web page captures.
  // The capture files used in these browser tests are also used on iOS to
  // test autofill.
  // The custom cert and key files are different from those of the official
  // WPR releases. The custom files are made to work on iOS.
  base::FilePath src_dir;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir)) {
    ADD_FAILURE() << "Failed to extract the Chromium source directory!";
    return false;
  }

  base::FilePath web_page_replay_support_file_dir =
      src_dir.AppendASCII("components")
          .AppendASCII("test")
          .AppendASCII("data")
          .AppendASCII("autofill")
          .AppendASCII("web_page_replay_support_files");
  full_command.AppendArg(base::StringPrintf(
      "--https_cert_file=%s,%s",
      FilePathToUTF8(
          web_page_replay_support_file_dir.AppendASCII("wpr_cert.pem").value())
          .c_str(),
      FilePathToUTF8(
          web_page_replay_support_file_dir.AppendASCII("ecdsa_cert.pem")
              .value())
          .c_str()));
  full_command.AppendArg(base::StringPrintf(
      "--https_key_file=%s,%s",
      FilePathToUTF8(
          web_page_replay_support_file_dir.AppendASCII("wpr_key.pem").value())
          .c_str(),
      FilePathToUTF8(
          web_page_replay_support_file_dir.AppendASCII("ecdsa_key.pem").value())
          .c_str()));

  for (const auto& arg : args)
    full_command.AppendArg(arg);

  VLOG(1) << full_command.GetArgumentsString();

  web_page_replay_server_ = base::LaunchProcess(full_command, options);
  return true;
}

// ProfileDataController ------------------------------------------------------
ProfileDataController::ProfileDataController()
    : profile_(autofill::test::GetIncompleteProfile2()),
      card_(autofill::CreditCard(
          base::Uuid::GenerateRandomV4().AsLowercaseString(),
          "http://www.example.com")) {

  // Initialize the credit card with default values, in case the test recipe
  // file does not contain pre-saved credit card info.
  autofill::test::SetCreditCardInfo(&card_, "Buddy Holly", "5187654321098765",
                                    "10", "2998", "1");
}

ProfileDataController::~ProfileDataController() = default;

bool ProfileDataController::AddAutofillProfileInfo(
    const std::string& field_type,
    const std::string& field_value) {
  std::optional<autofill::FieldType> type = StringToFieldType(field_type);
  if (!type.has_value()) {
    ADD_FAILURE() << "Unable to recognize autofill field type '" << field_type
                  << "'!";
    return false;
  }

  if (base::StartsWith(field_type, "HTML_TYPE_CREDIT_CARD_",
                       base::CompareCase::INSENSITIVE_ASCII) ||
      base::StartsWith(field_type, "CREDIT_CARD_",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    if (type == autofill::CREDIT_CARD_VERIFICATION_CODE) {
      cvc_ = base::UTF8ToUTF16(field_value);
    }
    card_.SetRawInfo(type.value(), base::UTF8ToUTF16(field_value));
  } else {
    profile_.SetRawInfo(type.value(), base::UTF8ToUTF16(field_value));
  }

  return true;
}

// TestRecipeReplayer ---------------------------------------------------------
TestRecipeReplayer::TestRecipeReplayer(
    Browser* browser,
    TestRecipeReplayChromeFeatureActionExecutor* feature_action_executor)
    : browser_(browser), feature_action_executor_(feature_action_executor) {
  CleanupSiteData();
  // Bypass permission dialogs.
  permissions::PermissionRequestManager::FromWebContents(GetWebContents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::ACCEPT_ALL);
}

TestRecipeReplayer::~TestRecipeReplayer() {
  // If there are still cookies at the time the browser test shuts down,
  // Chrome's SQL lite persistent cookie store will crash.
  CleanupSiteData();
  EXPECT_TRUE(web_page_replay_server_wrapper()->Stop())
      << "Cannot stop the local Web Page Replay server.";
}

bool TestRecipeReplayer::ReplayTest(
    const base::FilePath& capture_file_path,
    const base::FilePath& recipe_file_path,
    const std::optional<base::FilePath>& command_file_path) {
  logging::SetMinLogLevel(logging::LOGGING_WARNING);
  if (!web_page_replay_server_wrapper()->Start(capture_file_path))
    return false;
  if (OverrideAutofillClock(capture_file_path))
    VLOG(1) << "AutofillClock was set to:" << autofill::AutofillClock::Now();
  return ReplayRecordedActions(recipe_file_path, command_file_path);
}

const std::vector<testing::AssertionResult>
TestRecipeReplayer::GetValidationFailures() const {
  return validation_failures_;
}

// Extracts the time of the wpr recording from the wpr archive file and
// overrides the autofill::AutofillClock to match that time.
bool TestRecipeReplayer::OverrideAutofillClock(
    const base::FilePath capture_file_path) {
  std::string json_text;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!base::ReadFileToString(capture_file_path, &json_text)) {
      VLOG(1) << kClockNotSetMessage << "Could not read file";
      return false;
    }
  }
  // Decompress the json text from gzip.
  std::string decompressed_json_text;
  if (!compression::GzipUncompress(json_text, &decompressed_json_text)) {
    VLOG(1) << kClockNotSetMessage << "Could not gzip decompress file";
    return false;
  }
  // Convert the file text into a json object.
  std::optional<base::Value> parsed_json =
      base::JSONReader::Read(decompressed_json_text);
  if (!parsed_json) {
    VLOG(1) << kClockNotSetMessage << "Failed to deserialize json";
    return false;
  }

  const std::optional<double> time_value =
      parsed_json->GetDict().FindDouble("DeterministicTimeSeedMs");
  if (!time_value) {
    VLOG(1) << kClockNotSetMessage << "No DeterministicTimeSeedMs found";
    return false;
  }
  test_clock_.SetNow(base::Time::FromMillisecondsSinceUnixEpoch(*time_value));
  return true;
}

// static
void TestRecipeReplayer::SetUpHostResolverRules(
    base::CommandLine* command_line) {
  // Direct traffic to the Web Page Replay server.
  command_line->AppendSwitchASCII(
      network::switches::kHostResolverRules,
      base::StringPrintf(
          "MAP *:80 127.0.0.1:%d,"
          "MAP *:443 127.0.0.1:%d,"
          // Set to always exclude, allows cache_replayer overwrite
          "EXCLUDE clients1.google.com,"
          "EXCLUDE content-autofill.googleapis.com,"
          "EXCLUDE localhost",
          kHostHttpPort, kHostHttpsPort));
}

// static
void TestRecipeReplayer::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(
      network::switches::kIgnoreCertificateErrorsSPKIList,
      kWebPageReplayCertSPKI);
  command_line->AppendSwitch(switches::kStartMaximized);
  // Since we are adding via ScopedFeatureList for test features required, we
  // need to explicitly also enable field trials.
  command_line->AppendSwitch(
      variations::switches::kEnableFieldTrialTestingConfig);
}

TestRecipeReplayChromeFeatureActionExecutor*
TestRecipeReplayer::feature_action_executor() {
  return feature_action_executor_;
}

Browser* TestRecipeReplayer::browser() {
  return browser_;
}

WebPageReplayServerWrapper*
TestRecipeReplayer::web_page_replay_server_wrapper() {
  return web_page_replay_server_wrapper_.get();
}

content::WebContents* TestRecipeReplayer::GetWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void TestRecipeReplayer::WaitTillPageIsIdle(
    base::TimeDelta continuous_paint_timeout) {
  // Loop continually while WebContents are waiting for response or loading.
  // page_is_loading is expectedWaitTillPageIsIdle to always got to False
  // eventually, but adding a timeout as a fallback.
  base::TimeTicks finished_load_time = base::TimeTicks::Now();
  while (true) {
    {
      base::RunLoop heart_beat;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, heart_beat.QuitClosure(), wait_for_idle_loop_length);
      heart_beat.Run();
    }
    bool page_is_loading = GetWebContents()->IsWaitingForResponse() ||
                           GetWebContents()->IsLoading();
    if (!page_is_loading)
      break;
    if ((base::TimeTicks::Now() - finished_load_time) >
        continuous_paint_timeout) {
      VLOG(1) << "Page is still loading after "
              << visual_update_timeout.InSeconds()
              << " seconds. Bailing because timeout was reached.";
      break;
    }
  }
  finished_load_time = base::TimeTicks::Now();
  while (true) {
    // Now, rely on the render frame count to be the indicator of page activity.
    // Once all the frames are drawn, we're free to continue.
    content::RenderFrameSubmissionObserver frame_submission_observer(
        GetWebContents());
    {
      base::RunLoop heart_beat;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, heart_beat.QuitClosure(), wait_for_idle_loop_length);
      heart_beat.Run();
    }
    if (frame_submission_observer.render_frame_count() == 0) {
      // If the render r has stopped submitting frames
      break;
    } else if ((base::TimeTicks::Now() - finished_load_time) >
               continuous_paint_timeout) {
      // |continuous_paint_timeout| has expired since Chrome loaded the page.
      // During this period of time, Chrome has been continuously painting
      // the page. In this case, the page is probably idle, but a bug, a
      // blinking caret or a persistent animation is keeping the
      // |render_frame_count| from reaching zero. Exit.
      VLOG(1) << "Wait for render frame count timed out after "
              << continuous_paint_timeout.InSeconds()
              << " seconds with the frame count still at: "
              << frame_submission_observer.render_frame_count();
      break;
    }
  }
}

bool TestRecipeReplayer::WaitForVisualUpdate(base::TimeDelta timeout) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  content::RenderFrameSubmissionObserver frame_submission_observer(
      GetWebContents());
  while (frame_submission_observer.render_frame_count() == 0) {
    base::RunLoop heart_beat;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, heart_beat.QuitClosure(), wait_for_idle_loop_length);
    heart_beat.Run();
    if ((base::TimeTicks::Now() - start_time) > timeout) {
      return false;
    }
  }
  return true;
}

void TestRecipeReplayer::CleanupSiteData() {
  // Navigate to about:blank, then clear the browser cache.
  // Navigating to about:blank before clearing the cache ensures that
  // the cleanup is thorough and nothing is held.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser_, GURL(url::kAboutBlankURL)));
  content::BrowsingDataRemover* remover =
      browser_->profile()->GetBrowsingDataRemover();
  content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_COOKIES,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      &completion_observer);
  completion_observer.BlockUntilCompletion();
}

bool TestRecipeReplayer::ReplayRecordedActions(
    const base::FilePath& recipe_file_path,
    const std::optional<base::FilePath>& command_file_path) {
  // Read the text of the recipe file.
  base::ScopedAllowBlockingForTesting for_testing;
  std::string json_text;
  if (!base::ReadFileToString(recipe_file_path, &json_text)) {
    ADD_FAILURE() << "Failed to read recipe file '" << recipe_file_path << "'!";
    return false;
  }

  // Convert the file text into a json object.
  std::optional<base::Value> parsed_json = base::JSONReader::Read(json_text);
  if (!parsed_json) {
    ADD_FAILURE() << "Failed to deserialize json text!";
    return false;
  }

  DCHECK(parsed_json->is_dict());
  base::Value::Dict recipe = std::move(*parsed_json).TakeDict();
  if (!InitializeBrowserToExecuteRecipe(recipe))
    return false;

  // Iterate through and execute each action in the recipe.
  base::Value::List* action_list = recipe.FindList("actions");
  if (!action_list) {
    ADD_FAILURE() << "Failed to extract action list from the recipe!";
    return false;
  }

  ExecutionState execution_state{.length =
                                     static_cast<int>(action_list->size())};
  if (command_file_path.has_value()) {
    execution_state.limit = 0;  // Stop execution initially in debug mode.
    PrintDebugInstructions(command_file_path.value());
  }

  while (execution_state.index < execution_state.length) {
    if (execution_state.pause_on_failure &&
        (testing::Test::HasNonfatalFailure() ||
         testing::Test::HasFatalFailure() || validation_failures_.size() > 0)) {
      // If set to pause on a failure, move limit to current, but then reset
      // `pause_on_failure` so it can continue if the user requests.
      execution_state.limit = execution_state.index;
      execution_state.pause_on_failure = false;
    }
    if (command_file_path.has_value()) {
      while (execution_state.limit <= execution_state.index) {
        bool thread_finished = false;
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, {base::MayBlock()},
            base::BindOnce(&ProcessCommands, execution_state, action_list,
                           command_file_path.value()),
            base::BindOnce(
                [](ExecutionState* execution_state, bool* finished,
                   ExecutionState new_execution_state) {
                  *execution_state = new_execution_state;
                  *finished = true;
                },
                &execution_state, &thread_finished));
        while (!thread_finished) {
          base::RunLoop run_loop;
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
              FROM_HERE, run_loop.QuitClosure(), base::Seconds(1));
          run_loop.Run();
        }
      }
    }
    VLOG(1) << "Proceeding with execution with action " << execution_state.index
            << " of " << execution_state.length << ": "
            << (*action_list)[execution_state.index];

    if (!(*action_list)[execution_state.index].is_dict()) {
      ADD_FAILURE()
          << "Failed to extract an individual action from the recipe!";
      return false;
    }

    base::Value::Dict action =
        std::move((*action_list)[execution_state.index].GetDict());
    std::optional<std::string> type =
        FindPopulateString(action, "type", "action type");

    if (!type)
      return false;
    if (base::CompareCaseInsensitiveASCII(*type, "autofill") == 0) {
      if (!ExecuteAutofillAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "click") == 0) {
      if (!ExecuteClickAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "clickIfNotSeen") ==
               0) {
      if (!ExecuteClickIfNotSeenAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "closeTab") == 0) {
      if (!ExecuteCloseTabAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "coolOff") == 0) {
      if (!ExecuteCoolOffAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "executeScript") == 0) {
      if (!ExecuteRunCommandAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "hover") == 0) {
      if (!ExecuteHoverAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "loadPage") == 0) {
      if (!ExecuteForceLoadPage(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "pressEnter") == 0) {
      if (!ExecutePressEnterAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "pressEscape") == 0) {
      if (!ExecutePressEscapeAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "pressSpace") == 0) {
      if (!ExecutePressSpaceAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "savePassword") == 0) {
      if (!ExecuteSavePasswordAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "select") == 0) {
      if (!ExecuteSelectDropdownAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "type") == 0) {
      if (!ExecuteTypeAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "typePassword") == 0) {
      if (!ExecuteTypePasswordAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "updatePassword") ==
               0) {
      if (!ExecuteUpdatePasswordAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "validateField") == 0) {
      if (!ExecuteValidateFieldValueAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(
                   *type, "validatePasswordGenerationPrompt") == 0) {
      if (!ExecuteValidatePasswordGenerationPromptAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(
                   *type, "validateNoSavePasswordPrompt") == 0) {
      if (!ExecuteValidateNoSavePasswordPromptAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(
                   *type, "validatePasswordSaveFallback") == 0) {
      if (!ExecuteValidateSaveFallbackAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "waitFor") == 0) {
      if (!ExecuteWaitForStateAction(std::move(action)))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "breakpoint") == 0) {
      execution_state.limit = execution_state.index + 1;
    } else {
      ADD_FAILURE() << "Unrecognized action type: " << *type;
    }

    ++execution_state.index;
  }

  return true;
}

// Functions for deserializing and executing actions from the test recipe
// JSON object.
bool TestRecipeReplayer::InitializeBrowserToExecuteRecipe(
    base::Value::Dict& recipe) {
  // Setup any saved address and credit card at the start of the test.
  auto* autofill_profile_container = recipe.Find("autofillProfile");

  if (autofill_profile_container) {
    if (!autofill_profile_container->is_list()) {
      ADD_FAILURE() << "Save Autofill Profile is not a list!";
      return false;
    }

    if (!SetupSavedAutofillProfile(
            std::move(*autofill_profile_container).TakeList())) {
      return false;
    }
  }

  // Setup any saved passwords at the start of the test.
  auto* saved_password_container = recipe.Find("passwordManagerProfiles");

  if (saved_password_container) {
    if (!saved_password_container->is_list()) {
      ADD_FAILURE() << "Saved Password List is not a list!";
      return false;
    }

    if (!SetupSavedPasswords(std::move(*saved_password_container).TakeList())) {
      return false;
    }
  }

  // Extract the starting URL from the test recipe.
  auto* starting_url = recipe.FindString("startingURL");
  if (!starting_url) {
    ADD_FAILURE() << "Failed to extract the starting url from the recipe!";
    return false;
  }

  // Navigate to the starting URL, wait for the page to complete loading.
  if (!content::ExecJs(GetWebContents(),
                       base::StringPrintf("window.location.href = '%s';",
                                          starting_url->c_str()))) {
    ADD_FAILURE() << "Failed to navigate Chrome to '" << *starting_url << "!";
    return false;
  }

  WaitTillPageIsIdle();
  return true;
}

bool TestRecipeReplayer::ExecuteAutofillAction(base::Value::Dict action) {
  std::string xpath;
  content::RenderFrameHost* frame;
  if (!ExtractFrameAndVerifyElement(action, &xpath, &frame))
    return false;
  std::vector<std::string> frame_path;
  if (!GetIFramePathFromAction(action, &frame_path))
    return false;

  VLOG(1) << "Invoking Chrome Autofill on `" << xpath << "`.";
  // Clear the input box first, in case a previous value is there.
  // If the text input box is not clear, pressing the down key will not
  // bring up the autofill suggestion box.
  // This can happen on sites that requires the user to sign in. After
  // signing in, the site fills the form with the user's profile
  // information.
  if (!ExecuteJavaScriptOnElementByXpath(
          frame, xpath,
          "automation_helper.setInputElementValue(target, ``);")) {
    ADD_FAILURE() << "Failed to clear the input field value!";
    return false;
  }

  std::string autofill_triggered_field_type;
  if (GetElementProperty(frame, xpath,
                         "return target.getAttribute('autofill-prediction');",
                         &autofill_triggered_field_type)) {
    VLOG(1) << "The field's Chrome Autofill annotation: "
            << autofill_triggered_field_type << " during autofill form step.";
  } else {
    VLOG(1) << "Failed to obtain the field's Chrome Autofill annotation during "
               "autofill form step!";
  }
  if (!feature_action_executor()->AutofillForm(
          xpath, frame_path, kAutofillActionNumRetries, frame,
          StringToFieldType(autofill_triggered_field_type))) {
    return false;
  }
  WaitTillPageIsIdle(kAutofillActionWaitForVisualUpdateTimeout);
  return true;
}

bool TestRecipeReplayer::ExecuteClickAction(base::Value::Dict action) {
  std::string xpath;
  content::RenderFrameHost* frame;
  if (!ExtractFrameAndVerifyElement(action, &xpath, &frame))
    return false;

  VLOG(1) << "Left mouse clicking `" << xpath << "`.";
  if (!ScrollElementIntoView(xpath, frame))
    return false;
  WaitTillPageIsIdle(scroll_wait_timeout);

  gfx::Rect rect;
  if (!GetBoundingRectOfTargetElement(xpath, frame, &rect))
    return false;
  if (!SimulateLeftMouseClickAt(rect.CenterPoint(), frame))
    return false;

  WaitTillPageIsIdle();
  return true;
}

bool TestRecipeReplayer::ExecuteClickIfNotSeenAction(base::Value::Dict action) {
  std::string xpath;
  content::RenderFrameHost* frame;
  if (ExtractFrameAndVerifyElement(action, &xpath, &frame, false, false,
                                   true)) {
    return true;
  } else {
    // If the selector wasn't found, take the clickSelector and make it the
    // selector to attempt a click with that element instead.
    std::optional<std::string> click_xpath_text =
        FindPopulateString(action, "clickSelector", "click xpath selector");

    action.Set("selector", *click_xpath_text);

    return ExecuteClickAction(std::move(action));
  }
}

bool TestRecipeReplayer::ExecuteCloseTabAction(base::Value::Dict action) {
  VLOG(1) << "Closing Active Tab";
  browser_->tab_strip_model()->CloseSelectedTabs();
  return true;
}

bool TestRecipeReplayer::ExecuteCoolOffAction(base::Value::Dict action) {
  base::RunLoop heart_beat;
  base::TimeDelta cool_off_time = cool_off_action_timeout;
  base::Value* pause_time_container = action.Find("pauseTimeSec");
  if (pause_time_container) {
    if (!pause_time_container->is_int()) {
      ADD_FAILURE() << "Pause time is not an integer!";
      return false;
    }
    int seconds = pause_time_container->GetInt();
    cool_off_time = base::Seconds(seconds);
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, heart_beat.QuitClosure(), cool_off_time);
  VLOG(1) << "Pausing execution for '" << cool_off_time.InSeconds()
          << "' seconds";
  heart_beat.Run();

  return true;
}

bool TestRecipeReplayer::ExecuteHoverAction(base::Value::Dict action) {
  std::string xpath;
  content::RenderFrameHost* frame;
  if (!ExtractFrameAndVerifyElement(action, &xpath, &frame))
    return false;

  VLOG(1) << "Hovering over `" << xpath << "`.";

  if (!ScrollElementIntoView(xpath, frame))
    return false;
  WaitTillPageIsIdle(scroll_wait_timeout);

  gfx::Rect rect;
  if (!GetBoundingRectOfTargetElement(xpath, frame, &rect))
    return false;

  if (!SimulateMouseHoverAt(frame, rect.CenterPoint()))
    return false;

  if (!WaitForVisualUpdate()) {
    ADD_FAILURE() << "The page did not respond to a mouse hover action!";
    return false;
  }

  return true;
}

bool TestRecipeReplayer::ExecuteForceLoadPage(base::Value::Dict action) {
  bool should_force = action.FindBool("force").value_or(false);
  if (!should_force) {
    return true;
  }

  std::optional<std::string> url =
      FindPopulateString(action, "url", "Force Load URL");
  if (!url)
    return false;
  VLOG(1) << "Making explicit URL redirect to '" << *url << "'";
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser_, GURL(*url)));

  WaitTillPageIsIdle();

  return true;
}

bool TestRecipeReplayer::ExecutePressEnterAction(base::Value::Dict action) {
  std::string xpath;
  content::RenderFrameHost* frame;
  if (!ExtractFrameAndVerifyElement(action, &xpath, &frame))
    return false;

  VLOG(1) << "Pressing 'Enter' on `" << xpath << "`.";
  SimulateKeyPressWrapper(content::WebContents::FromRenderFrameHost(frame),
                          ui::DomKey::ENTER);
  WaitTillPageIsIdle();
  return true;
}

bool TestRecipeReplayer::ExecutePressEscapeAction(base::Value::Dict action) {
  VLOG(1) << "Pressing 'Esc' in the current frame";
  SimulateKeyPressWrapper(GetWebContents(), ui::DomKey::ESCAPE);
  WaitTillPageIsIdle();
  return true;
}

bool TestRecipeReplayer::ExecutePressSpaceAction(base::Value::Dict action) {
  std::string xpath;
  content::RenderFrameHost* frame;
  if (!ExtractFrameAndVerifyElement(action, &xpath, &frame, true))
    return false;

  VLOG(1) << "Pressing 'Space' on `" << xpath << "`.";
  SimulateKeyPressWrapper(content::WebContents::FromRenderFrameHost(frame),
                          ui::DomKey::FromCharacter(' '));
  WaitTillPageIsIdle();
  return true;
}

bool TestRecipeReplayer::ExecuteRunCommandAction(base::Value::Dict action) {
  // Extract the list of JavaScript commands into a vector.
  std::vector<std::string> commands;

  base::Value::List* list = action.FindList("commands");
  if (!list) {
    ADD_FAILURE() << "Failed to extract commands list from action";
    return false;
  }

  for (const auto& command : *list) {
    if (!command.is_string()) {
      ADD_FAILURE() << "command is not a string: " << command;
      return false;
    }
    commands.push_back(command.GetString());
  }

  content::RenderFrameHost* frame;
  if (!GetTargetFrameFromAction(action, &frame)) {
    return false;
  }

  VLOG(1) << "Running JavaScript commands on the page.";

  // Execute the commands.
  for (const std::string& command : commands) {
    if (!content::ExecJs(frame, command)) {
      ADD_FAILURE() << "Failed to execute JavaScript command `" << command
                    << "`!";
      return false;
    }
    // Wait in case the JavaScript command triggers page load or layout
    // changes.
    WaitTillPageIsIdle();
  }

  return true;
}

bool TestRecipeReplayer::ExecuteSavePasswordAction(base::Value::Dict action) {
  VLOG(1) << "Save password.";

  if (!feature_action_executor()->SavePassword())
    return false;

  bool stored_cred;
  if (!HasChromeStoredCredential(action, &stored_cred))
    return false;

  if (!stored_cred) {
    ADD_FAILURE() << "Chrome did not save the credential!";
    return false;
  }

  return true;
}

bool TestRecipeReplayer::ExecuteSelectDropdownAction(base::Value::Dict action) {
  std::optional<int> index = action.FindInt("index");
  if (!index.has_value()) {
    ADD_FAILURE() << "Failed to extract Selection Index from action";
    return false;
  }

  std::string xpath;
  content::RenderFrameHost* frame;
  if (!ExtractFrameAndVerifyElement(action, &xpath, &frame))
    return false;

  VLOG(1) << "Select option '" << *index << "' from `" << xpath << "`.";
  if (!ExecuteJavaScriptOnElementByXpath(
          frame, xpath,
          base::StringPrintf(
              "automation_helper"
              "  .selectOptionFromDropDownElementByIndex(target, %d);",
              *index))) {
    ADD_FAILURE() << "Failed to select drop down option with JavaScript!";
    return false;
  }
  WaitTillPageIsIdle();
  return true;
}

bool TestRecipeReplayer::ExecuteTypeAction(base::Value::Dict action) {
  std::optional<std::string> value =
      FindPopulateString(action, "value", "typing value");
  if (!value)
    return false;

  std::string xpath;
  content::RenderFrameHost* frame;
  if (!ExtractFrameAndVerifyElement(action, &xpath, &frame))
    return false;

  VLOG(1) << "Typing '" << *value << "' inside `" << xpath << "`.";
  if (!ExecuteJavaScriptOnElementByXpath(
          frame, xpath,
          base::StringPrintf(
              "automation_helper.setInputElementValue(target, `%s`);",
              value->c_str()))) {
    ADD_FAILURE() << "Failed to type inside input element with JavaScript!";
    return false;
  }
  WaitTillPageIsIdle();
  return true;
}

bool TestRecipeReplayer::ExecuteTypePasswordAction(base::Value::Dict action) {
  std::string xpath;
  content::RenderFrameHost* frame;
  if (!ExtractFrameAndVerifyElement(action, &xpath, &frame, true))
    return false;

  std::optional<std::string> value =
      FindPopulateString(action, "value", "password text");
  if (!value)
    return false;

  // Clear the password field first, in case a previous value is there.
  if (!ExecuteJavaScriptOnElementByXpath(
          frame, xpath,
          "automation_helper.setInputElementValue(target, ``);")) {
    ADD_FAILURE() << "Failed to execute JavaScript to clear the input value!";
    return false;
  }

  VLOG(1) << "Typing '" << *value << "' inside `" << xpath << "`.";
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);
  for (size_t index = 0; index < value->size(); index++) {
    SimulateKeyPressWrapper(web_contents,
                            ui::DomKey::FromCharacter(value->at(index)));
  }
  WaitTillPageIsIdle();
  return true;
}

bool TestRecipeReplayer::ExecuteUpdatePasswordAction(base::Value::Dict action) {
  VLOG(1) << "Update password.";

  if (!feature_action_executor()->UpdatePassword())
    return false;

  bool stored_cred;
  if (!HasChromeStoredCredential(action, &stored_cred))
    return false;

  if (!stored_cred) {
    ADD_FAILURE() << "Chrome did not update the credential!";
    return false;
  }

  return true;
}

bool TestRecipeReplayer::ExecuteValidateFieldValueAction(
    base::Value::Dict action) {
  std::string xpath;
  content::RenderFrameHost* frame;
  if (!ExtractFrameAndVerifyElement(action, &xpath, &frame, false, true))
    return false;

  base::Value* autofill_prediction_container =
      action.Find("expectedAutofillType");
  if (autofill_prediction_container) {
    if (!autofill_prediction_container->is_string()) {
      ADD_FAILURE() << "Autofill prediction is not a string!";
      return false;
    }

    // If we are validating the value of a Chrome autofilled field, print the
    // Chrome Autofill's field annotation for debugging purpose.
    std::string autofill_information;
    if (GetElementProperty(
            frame, xpath, "return target.getAttribute('autofill-information');",
            &autofill_information)) {
      VLOG(1) << autofill_information;
    } else {
      // Only used for logging purposes, so don't ADD_FAILURE() if it fails.
      VLOG(1) << "Failed to obtain the field's Chrome Autofill annotation!";
    }

    std::string expected_autofill_prediction_type =
        autofill_prediction_container->GetString();
    VLOG(1) << "Checking the field `" << xpath << "` has the autofill type '"
            << expected_autofill_prediction_type << "'";
    ExpectElementPropertyEqualsAnyOf(
        frame, xpath, "return target.getAttribute('autofill-prediction');",
        {expected_autofill_prediction_type}, "autofill type mismatch",
        IgnoreCase(true));
  }

  std::optional<std::vector<std::string>> expected_values =
      FindPopulateStringVector(action, "expectedValues", AllowNull());
  std::optional<std::string> expected_value =
      FindPopulateString(action, "expectedValue", AllowNull());
  if (!!expected_values == !!expected_value) {
    ADD_FAILURE() << "Failed to extract 'expectedValue' xor 'expectedValues' "
                     "vector from container !";
    return false;
  }

  VLOG(1) << "Checking the field `" << xpath << "`.";
  if (expected_value) {
    ExpectElementPropertyEqualsAnyOf(frame, xpath, "return target.value;",
                                     {*expected_value}, "text value mismatch");
  }
  if (expected_values) {
    ExpectElementPropertyEqualsAnyOf(frame, xpath, "return target.value;",
                                     *expected_values, "text value mismatch");
  }
  return true;
}

bool TestRecipeReplayer::ExecuteValidateNoSavePasswordPromptAction(
    base::Value::Dict action) {
  VLOG(1) << "Verify that the page hasn't shown a save password prompt.";
  EXPECT_FALSE(feature_action_executor()->HasChromeShownSavePasswordPrompt());
  return true;
}

bool TestRecipeReplayer::ExecuteValidatePasswordGenerationPromptAction(
    base::Value::Dict action) {
  VLOG(1) << "Verify that an element is properly displaying or not displaying "
             "the password generation prompt";
  std::string xpath;
  content::RenderFrameHost* frame;
  if (!ExtractFrameAndVerifyElement(action, &xpath, &frame, true))
    return false;

  // Most common scenario is validating that the password generation prompt is
  // being shown, so if unspecified default to true.
  bool expect_to_be_shown = expect_to_be_shown =
      action.FindBool("shouldBeShown").value_or(true);

  // First, execute a click to focus on the field in question.
  ExecuteClickAction(std::move(action));

  // Validate that the password generation prompt is shown when expected.
  ValidatePasswordGenerationPromptState(frame, xpath, expect_to_be_shown);
  return true;
}

void TestRecipeReplayer::ValidatePasswordGenerationPromptState(
    const content::ToRenderFrameHost& frame,
    const std::string& element_xpath,
    bool expect_to_be_shown) {
  bool is_password_generation_prompt_showing =
      feature_action_executor()->IsChromeShowingPasswordGenerationPrompt();

  if (expect_to_be_shown != is_password_generation_prompt_showing) {
    std::string error_message =
        base::StrCat({"Field xpath: `", element_xpath, "` is",
                      is_password_generation_prompt_showing ? "" : " not",
                      " showing password generation prompt but it should",
                      expect_to_be_shown ? "" : " not", " be."});
    VLOG(1) << error_message;
    ADD_FAILURE() << error_message;
    validation_failures_.push_back(testing::AssertionFailure()
                                   << error_message);
  }
}

bool TestRecipeReplayer::ExecuteValidateSaveFallbackAction(
    base::Value::Dict action) {
  VLOG(1) << "Verify that Chrome shows the save fallback icon in the omnibox.";
  EXPECT_TRUE(feature_action_executor()->WaitForSaveFallback());
  return true;
}

bool TestRecipeReplayer::ExecuteWaitForStateAction(base::Value::Dict action) {
  // Extract the list of JavaScript assertions into a vector.
  std::vector<std::string> state_assertions;

  base::Value::List* list = action.FindList("assertions");
  if (!list) {
    ADD_FAILURE() << "Failed to extract wait assertions list from action";
    return false;
  }
  for (const base::Value& assertion : *list) {
    if (!assertion.is_string()) {
      ADD_FAILURE() << "Assertion is not a string: " << assertion;
      return false;
    }
    state_assertions.push_back(assertion.GetString());
  }

  content::RenderFrameHost* frame;
  if (!GetTargetFrameFromAction(action, &frame))
    return false;

  VLOG(1) << "Waiting for page to reach a state.";

  // Wait for all of the assertions to become true on the current page.
  return WaitForStateChange(frame, state_assertions, default_action_timeout);
}

bool TestRecipeReplayer::GetTargetHTMLElementXpathFromAction(
    const base::Value::Dict& action,
    std::string* xpath) {
  xpath->clear();
  std::optional<std::string> xpath_text =
      FindPopulateString(action, "selector", "xpath selector");
  if (!xpath_text)
    return false;
  *xpath = *xpath_text;
  return true;
}

bool TestRecipeReplayer::GetTargetHTMLElementVisibilityEnumFromAction(
    const base::Value::Dict& action,
    int* visibility_enum_val) {
  const base::Value* visibility_container = action.Find("visibility");
  if (!visibility_container) {
    // By default, set the visibility to (visible | enabled | on_top), as
    // defined in
    // chrome/test/data/web_page_replay_go_helper_scripts/automation_helper.js
    *visibility_enum_val = 7;
    return true;
  }

  if (!visibility_container->is_int()) {
    ADD_FAILURE() << "visibility property is not an integer!";
    return false;
  }

  *visibility_enum_val = visibility_container->GetInt();
  return true;
}

bool TestRecipeReplayer::GetTargetFrameFromAction(
    const base::Value::Dict& action,
    content::RenderFrameHost** frame) {
  const base::Value* iframe_container = action.Find("context");
  if (!iframe_container) {
    ADD_FAILURE() << "Failed to extract the iframe context from action!";
    return false;
  }

  if (!iframe_container->is_dict()) {
    ADD_FAILURE() << "Failed to extract the iframe context object!";
    return false;
  }

  std::optional<bool> is_iframe_container =
      iframe_container->GetDict().FindBool("isIframe");
  if (!is_iframe_container) {
    ADD_FAILURE() << "Failed to extract isIframe from the iframe context! ";
    return false;
  }
  if (!is_iframe_container.value_or(false)) {
    *frame = GetWebContents()->GetPrimaryMainFrame();
    return true;
  }

  const base::Value* frame_name_container =
      iframe_container->GetDict().FindByDottedPath("browserTest.name");
  const base::Value* frame_origin_container =
      iframe_container->GetDict().FindByDottedPath("browserTest.origin");
  const base::Value* frame_url_container =
      iframe_container->GetDict().FindByDottedPath("browserTest.url");
  IFrameWaiter iframe_waiter(GetWebContents());

  if (frame_name_container != nullptr && !frame_name_container->is_string()) {
    ADD_FAILURE() << "Iframe name is not a string!";
    return false;
  }

  if (frame_origin_container != nullptr &&
      !frame_origin_container->is_string()) {
    ADD_FAILURE() << "Iframe origin is not a string!";
    return false;
  }

  if (frame_url_container != nullptr && !frame_url_container->is_string()) {
    ADD_FAILURE() << "Iframe url is not a string!";
    return false;
  }

  if (frame_name_container != nullptr) {
    std::string frame_name = frame_name_container->GetString();
    *frame = iframe_waiter.WaitForFrameMatchingName(frame_name);
  } else if (frame_origin_container != nullptr) {
    std::string frame_origin = frame_origin_container->GetString();
    *frame = iframe_waiter.WaitForFrameMatchingOrigin(GURL(frame_origin));
  } else if (frame_url_container != nullptr) {
    std::string frame_url = frame_url_container->GetString();
    *frame = iframe_waiter.WaitForFrameMatchingUrl(GURL(frame_url));
  } else {
    ADD_FAILURE() << "The recipe does not specify a way to find the iframe!";
  }

  if (*frame == nullptr) {
    ADD_FAILURE() << "Failed to find iframe!";
    return false;
  }

  return true;
}

bool TestRecipeReplayer::ExtractFrameAndVerifyElement(
    const base::Value::Dict& action,
    std::string* xpath,
    content::RenderFrameHost** frame,
    bool set_focus,
    bool relaxed_visibility,
    bool ignore_failure) {
  if (!GetTargetHTMLElementXpathFromAction(action, xpath))
    return false;

  int visibility_enum_val;
  if (!GetTargetHTMLElementVisibilityEnumFromAction(action,
                                                    &visibility_enum_val))
    return false;
  if (!GetTargetFrameFromAction(action, frame))
    return false;

  // If we're just validating we don't care about on_top-ness, as copied from
  // chrome/test/data/web_page_replay_go_helper_scripts/automation_helper.js
  // to TestRecipeReplayer::DomElementReadyState enum
  // So remove (DomElementReadyState::kReadyStateOnTop)
  if (relaxed_visibility)
    visibility_enum_val &= ~kReadyStateOnTop;

  if (!WaitForElementToBeReady(*xpath, visibility_enum_val, *frame,
                               ignore_failure))
    return false;

  if (set_focus) {
    std::vector<std::string> frame_path;
    if (!GetIFramePathFromAction(action, &frame_path))
      return false;

    if (!PlaceFocusOnElement(*xpath, frame_path, *frame))
      return false;
  }
  return true;
}

bool TestRecipeReplayer::GetIFramePathFromAction(
    const base::Value::Dict& action,
    std::vector<std::string>* iframe_path) {
  *iframe_path = std::vector<std::string>();

  const base::Value* iframe_container = action.Find("context");
  if (!iframe_container) {
    ADD_FAILURE() << "Failed to extract the iframe context from action!";
    return false;
  }

  if (!iframe_container->is_dict()) {
    ADD_FAILURE() << "Failed to extract the iframe context object!";
    return false;
  }

  const base::Value* iframe_path_container =
      iframe_container->GetDict().Find("path");
  if (!iframe_path_container) {
    // If the action does not have a path container, it would mean that:
    // 1. The target frame is the top level frame.
    // 2. The target frame is an iframe, but it is the top-level frame in its
    //    rendering process.
    return true;
  }
  if (!iframe_path_container->is_list()) {
    ADD_FAILURE() << "The action's iframe path is not a list!";
    return false;
  }
  for (const auto& xpath : iframe_path_container->GetList()) {
    if (!xpath.is_string()) {
      ADD_FAILURE() << "Failed to extract the iframe xpath from action!";
      return false;
    }
    iframe_path->push_back(xpath.GetString());
  }
  return true;
}

bool TestRecipeReplayer::GetIFrameOffsetFromIFramePath(
    const std::vector<std::string>& iframe_path,
    content::RenderFrameHost* frame,
    gfx::Vector2d* offset) {
  *offset = gfx::Vector2d(0, 0);

  for (auto it_xpath = iframe_path.begin(); it_xpath != iframe_path.end();
       it_xpath++) {
    content::RenderFrameHost* parent_frame = frame->GetParent();
    if (parent_frame == nullptr) {
      ADD_FAILURE() << "Trying to iterate past the top level frame!";
      return false;
    }

    gfx::Rect rect;
    if (!GetBoundingRectOfTargetElement(*it_xpath, parent_frame, &rect)) {
      ADD_FAILURE() << "Failed to extract position of iframe with xpath `"
                    << *it_xpath << "`!";
      return false;
    }

    *offset += rect.OffsetFromOrigin();
    frame = parent_frame;
  }

  return true;
}

bool TestRecipeReplayer::WaitForElementToBeReady(
    const std::string& xpath,
    const int visibility_enum_val,
    content::RenderFrameHost* frame,
    bool ignore_failure) {
  std::vector<std::string> state_assertions;
  state_assertions.push_back(base::StringPrintf(
      "return automation_helper.isElementWithXpathReady(`%s`, %d);",
      xpath.c_str(), visibility_enum_val));
  return WaitForStateChange(
      frame, state_assertions,
      ignore_failure ? click_fallback_timeout : default_action_timeout,
      ignore_failure);
}

bool TestRecipeReplayer::WaitForStateChange(
    content::RenderFrameHost* frame,
    const std::vector<std::string>& state_assertions,
    const base::TimeDelta& timeout,
    bool ignore_failure) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  while (!AllAssertionsPassed(frame, state_assertions)) {
    if (base::TimeTicks::Now() - start_time > timeout) {
      if (!ignore_failure) {
        ADD_FAILURE() << "State change hasn't completed within timeout.";
      }
      return false;
    }
    WaitTillPageIsIdle();
  }
  return true;
}

bool TestRecipeReplayer::AllAssertionsPassed(
    const content::ToRenderFrameHost& frame,
    const std::vector<std::string>& assertions) {
  for (const std::string& assertion : assertions) {
    if (!EvalJs(frame, base::StringPrintf("(function() {"
                                          "  try {"
                                          "    %s"
                                          "  } catch (ex) {}"
                                          "  return false;"
                                          "})();",
                                          assertion.c_str()))
             .ExtractBool()) {
      VLOG(1) << "'" << assertion << "' failed!";
      return false;
    }
  }
  return true;
}

bool TestRecipeReplayer::ExecuteJavaScriptOnElementByXpath(
    const content::ToRenderFrameHost& frame,
    const std::string& element_xpath,
    const std::string& execute_function_body,
    const base::TimeDelta& time_to_wait_for_element) {
  std::string js(base::StringPrintf(
      "try {"
      "  var element = automation_helper.getElementByXpath(`%s`);"
      "  (function(target) { %s })(element);"
      "} catch(ex) {}",
      element_xpath.c_str(), execute_function_body.c_str()));
  return ExecJs(frame, js);
}

bool TestRecipeReplayer::GetElementProperty(
    const content::ToRenderFrameHost& frame,
    const std::string& element_xpath,
    const std::string& get_property_function_body,
    std::string* property) {
  content::EvalJsResult result = content::EvalJs(
      frame, base::StringPrintf(
                 "(function() {"
                 "    var element = function() {"
                 "      return automation_helper.getElementByXpath(`%s`);"
                 "    }();"
                 "    return function(target){%s}(element);})();",
                 element_xpath.c_str(), get_property_function_body.c_str()));
  if (result.error.empty() && result.value.is_string()) {
    *property = result.ExtractString();
    return true;
  }
  *property = result.error;
  return false;
}

bool TestRecipeReplayer::ExpectElementPropertyEqualsAnyOf(
    const content::ToRenderFrameHost& frame,
    const std::string& element_xpath,
    const std::string& get_property_function_body,
    const std::vector<std::string>& expected_values,
    const std::string& validation_field,
    IgnoreCase ignore_case) {
  std::string actual_value;
  if (!GetElementProperty(frame, element_xpath, get_property_function_body,
                          &actual_value)) {
    ADD_FAILURE() << "Failed to extract element property! " << element_xpath
                  << ", " << get_property_function_body;
    return false;
  }

  auto is_expected = [ignore_case,
                      &actual_value](std::string_view expected_value) {
    return ignore_case
               ? base::EqualsCaseInsensitiveASCII(expected_value, actual_value)
               : expected_value == actual_value;
  };

  if (std::ranges::none_of(expected_values, is_expected)) {
    std::string error_message = base::StrCat(
        {"Field xpath: `", element_xpath, "` ", validation_field, ", ",
         "Expected: '", base::JoinString(expected_values, " or "),
         "', actual: '", actual_value, "'"});
    VLOG(1) << error_message;
    validation_failures_.push_back(testing::AssertionFailure()
                                   << error_message);
  }
  return true;
}

bool TestRecipeReplayer::ScrollElementIntoView(
    const std::string& element_xpath,
    content::RenderFrameHost* frame) {
  const std::string scroll_target_js(base::StringPrintf(
      "try {"
      "  const element = automation_helper.getElementByXpath(`%s`);"
      "  element.scrollIntoView({"
      "    block: 'center', inline: 'center'});"
      "  true;"
      "} catch(ex) {"
      "  false;"
      "}",
      element_xpath.c_str()));

  return EvalJs(frame, scroll_target_js).ExtractBool();
}

bool TestRecipeReplayer::PlaceFocusOnElement(
    const std::string& element_xpath,
    const std::vector<std::string>& iframe_path,
    content::RenderFrameHost* frame) {
  if (!ScrollElementIntoView(element_xpath, frame))
    return false;

  const std::string focus_on_target_field_js(
      base::StringPrintf("(function() {const element = "
                         "automation_helper.getElementByXpath(`%s`);"
                         "    if (document.activeElement !== element) {"
                         "      element.focus();"
                         "    }"
                         "    return document.activeElement === element;})();",
                         element_xpath.c_str()));

  content::EvalJsResult result =
      content::EvalJs(frame, focus_on_target_field_js);
  if (result.error.empty() && result.value.is_bool() && result.ExtractBool()) {
    return true;
  } else {
    VLOG(1) << "Failed to focus element through script:"
            << (result.error.empty()
                    ? (result.value.is_bool() ? "Not a valid bool"
                                              : "Returned false")
                    : result.error);

    // Failing focusing on an element through script, use the less preferred
    // method of left mouse clicking the element.
    gfx::Rect rect;
    if (!GetBoundingRectOfTargetElement(element_xpath, iframe_path, frame,
                                        &rect))
      return false;

    return SimulateLeftMouseClickAt(rect.CenterPoint(), frame);
  }
}

bool TestRecipeReplayer::GetBoundingRectOfTargetElement(
    const std::string& target_element_xpath,
    content::RenderFrameHost* frame,
    gfx::Rect* output_rect) {
  const std::string get_element_bounding_rect_js(base::StringPrintf(
      "(function() {"
      "   try {"
      "     const element = automation_helper.getElementByXpath(`%s`);"
      "     const rect = element.getBoundingClientRect();"
      "     return Math.round(rect.left) + ',' + "
      "            Math.round(rect.top) + ',' + "
      "            Math.round(rect.width) + ',' + "
      "            Math.round(rect.height);"
      "   } catch(ex) {}"
      "   return '';"
      "})();",
      target_element_xpath.c_str()));

  std::string rect_str =
      content::EvalJs(frame, get_element_bounding_rect_js).ExtractString();

  if (rect_str.empty()) {
    ADD_FAILURE() << "Failed to extract target element's bounding rect!";
    return false;
  }

  // Parse the bounding rect string to extract the element coordinates.
  std::vector<std::string_view> rect_components = base::SplitStringPiece(
      rect_str, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (rect_components.size() != 4) {
    ADD_FAILURE() << "Wrong number of components in `" << rect_str << "`!";
    return false;
  }

  int x = 0;
  if (!base::StringToInt(rect_components[0], &x)) {
    ADD_FAILURE() << "Failed to extract target element's x coordinate from "
                  << "the string `" << rect_str[0] << "`!";
    return false;
  }
  int y = 0;
  if (!base::StringToInt(rect_components[1], &y)) {
    ADD_FAILURE() << "Failed to extract target element's y coordinate from "
                  << "the string `" << rect_str[1] << "`!";
    return false;
  }
  int width = 0;
  if (!base::StringToInt(rect_components[2], &width)) {
    ADD_FAILURE() << "Failed to extract target element's width from "
                  << "the string `" << rect_str[2] << "`!";
    return false;
  }
  int height = 0;
  if (!base::StringToInt(rect_components[3], &height)) {
    ADD_FAILURE() << "Failed to extract target element's height from "
                  << "the string `" << rect_str[3] << "`!";
    return false;
  }

  output_rect->set_x(x);
  output_rect->set_y(y);
  output_rect->set_width(width);
  output_rect->set_height(height);

  return true;
}

bool TestRecipeReplayer::GetBoundingRectOfTargetElement(
    const std::string& target_element_xpath,
    const std::vector<std::string>& iframe_path,
    content::RenderFrameHost* frame,
    gfx::Rect* output_rect) {
  gfx::Vector2d offset;
  if (!GetIFrameOffsetFromIFramePath(iframe_path, frame, &offset))
    return false;
  if (!GetBoundingRectOfTargetElement(target_element_xpath, frame, output_rect))
    return false;

  *output_rect += offset;
  return true;
}

bool TestRecipeReplayer::SimulateLeftMouseClickAt(
    const gfx::Point& point,
    content::RenderFrameHost* render_frame_host) {
  content::RenderWidgetHostView* view = render_frame_host->GetView();
  if (!SimulateMouseHoverAt(render_frame_host, point))
    return false;

  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(point.x(), point.y());

  // Mac needs positionInScreen for events to plugins.
  gfx::Rect offset =
      content::WebContents::FromRenderFrameHost(render_frame_host)
          ->GetContainerBounds();
  mouse_event.SetPositionInScreen(point.x() + offset.x(),
                                  point.y() + offset.y());
  mouse_event.click_count = 1;
  content::RenderWidgetHost* widget = view->GetRenderWidgetHost();

  widget->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  widget->ForwardMouseEvent(mouse_event);
  return true;
}

bool TestRecipeReplayer::SimulateMouseHoverAt(
    content::RenderFrameHost* render_frame_host,
    const gfx::Point& point) {
  gfx::Rect offset =
      content::WebContents::FromRenderFrameHost(render_frame_host)
          ->GetContainerBounds();
  gfx::Point reset_mouse =
      gfx::Point(offset.x() + point.x(), offset.y() + point.y());
  if (!ui_test_utils::SendMouseMoveSync(reset_mouse)) {
    ADD_FAILURE() << "Failed to position the mouse!";
    return false;
  }
  return true;
}

void TestRecipeReplayer::SimulateKeyPressWrapper(
    content::WebContents* web_contents,
    ui::DomKey key) {
  ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
  ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  SimulateKeyPress(web_contents, key, code, key_code, false, false, false,
                   false);
}

bool TestRecipeReplayer::HasChromeStoredCredential(
    const base::Value::Dict& action,
    bool* stored_cred) {
  std::optional<std::string> origin =
      FindPopulateString(action, "origin", "Origin");
  std::optional<std::string> username =
      FindPopulateString(action, "userName", "Username");
  std::optional<std::string> password =
      FindPopulateString(action, "password", "Password");
  if (!origin || !username || !password)
    return false;
  *stored_cred = feature_action_executor()->HasChromeStoredCredential(
      *origin, *username, *password);

  return true;
}

bool TestRecipeReplayer::SetupSavedAutofillProfile(
    base::Value::List saved_autofill_profile_container) {
  for (auto& list_entry : saved_autofill_profile_container) {
    if (!list_entry.is_dict()) {
      ADD_FAILURE() << "Failed to extract an entry!";
      return false;
    }

    const base::Value::Dict list_entry_dict = std::move(list_entry).TakeDict();
    std::optional<std::string> type =
        FindPopulateString(list_entry_dict, "type", "profile field type");
    std::optional<std::string> value =
        FindPopulateString(list_entry_dict, "value", "profile field value");

    if (!type || !value)
      return false;

    if (!feature_action_executor()->AddAutofillProfileInfo(*type, *value)) {
      return false;
    }
  }

  // Skip this step if autofill profile is empty.
  // Only Autofill Captured Sites test recipes will have non-empty autofill
  // profiles. Recipes for other captured sites tests will have empty autofill
  // profiles. This block prevents these other tests from failing because
  // the test feature action executor does not know how to setup the autofill
  // profile.
  if (saved_autofill_profile_container.empty()) {
    return true;
  }

  return feature_action_executor()->SetupAutofillProfile();
}

bool TestRecipeReplayer::SetupSavedPasswords(
    base::Value::List saved_password_list_container) {
  for (auto& entry : saved_password_list_container) {
    if (!entry.is_dict()) {
      ADD_FAILURE() << "Failed to extract a saved password!";
      return false;
    }

    const base::Value::Dict entry_dict = std::move(entry.GetDict());

    std::optional<std::string> origin =
        FindPopulateString(entry_dict, "website", "Website");
    std::optional<std::string> username =
        FindPopulateString(entry_dict, "username", "Username");
    std::optional<std::string> password =
        FindPopulateString(entry_dict, "password", "Password");
    if (!origin || !username || !password)
      return false;

    if (!feature_action_executor()->AddCredential(*origin, *username,
          *password)) {
      return false;
    }
  }

  return true;
}

// TestRecipeReplayChromeFeatureActionExecutor --------------------------------
TestRecipeReplayChromeFeatureActionExecutor::
    TestRecipeReplayChromeFeatureActionExecutor() {}
TestRecipeReplayChromeFeatureActionExecutor::
    ~TestRecipeReplayChromeFeatureActionExecutor() {}

bool TestRecipeReplayChromeFeatureActionExecutor::AutofillForm(
    const std::string& focus_element_css_selector,
    const std::vector<std::string>& iframe_path,
    const int attempts,
    content::RenderFrameHost* frame,
    std::optional<autofill::FieldType> triggered_field_type) {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor::AutofillForm "
                   "is not implemented!";
  return false;
}

bool TestRecipeReplayChromeFeatureActionExecutor::AddAutofillProfileInfo(
    const std::string& field_type,
    const std::string& field_value) {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor"
                   "::AddAutofillProfileInfo is not implemented!";
  return false;
}

bool TestRecipeReplayChromeFeatureActionExecutor::SetupAutofillProfile() {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor"
                   "::SetupAutofillProfile is not implemented!";
  return false;
}

bool TestRecipeReplayChromeFeatureActionExecutor::AddCredential(
    const std::string& origin,
    const std::string& username,
    const std::string& password) {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor::AddCredential"
                   " is not implemented!";
  return false;
}

bool TestRecipeReplayChromeFeatureActionExecutor::SavePassword() {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor::SavePassword"
                   " is not implemented!";
  return false;
}

bool TestRecipeReplayChromeFeatureActionExecutor::UpdatePassword() {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor"
                   "::UpdatePassword is not implemented!";
  return false;
}

bool TestRecipeReplayChromeFeatureActionExecutor::WaitForSaveFallback() {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor"
                   "::WaitForSaveFallback is not implemented!";
  return false;
}

bool TestRecipeReplayChromeFeatureActionExecutor::
    IsChromeShowingPasswordGenerationPrompt() {
  ADD_FAILURE()
      << "TestRecipeReplayChromeFeatureActionExecutor"
         "::IsChromeShowingPasswordGenerationPrompt is not implemented!";
  return false;
}

bool TestRecipeReplayChromeFeatureActionExecutor::
    HasChromeShownSavePasswordPrompt() {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor"
                   "::HasChromeShownSavePasswordPrompt is not implemented!";
  return false;
}

bool TestRecipeReplayChromeFeatureActionExecutor::HasChromeStoredCredential(
    const std::string& origin,
    const std::string& username,
    const std::string& password) {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor"
                   "::HasChromeStoredCredential is not implemented!";
  return false;
}

}  // namespace captured_sites_test_utils
