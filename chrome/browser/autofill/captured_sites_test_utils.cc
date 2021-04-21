// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/captured_sites_test_utils.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_handle.h"
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

// The maximum amount of time to wait for Chrome to finish autofilling a form.
const base::TimeDelta kAutofillActionWaitForVisualUpdateTimeout =
    base::TimeDelta::FromSeconds(3);

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
  $ echo help    >%1$s  # prints this text
)";
  LOG(INFO) << base::StringPrintf(msg,
                                  command_file_path.AsUTF8Unsafe().c_str());
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
        if (space == base::StringPiece::npos)
          return default_value;
        int value;
        if (!base::StringToInt(command.substr(space + 1), &value))
          return default_value;
        return value;
      };

      if (base::StartsWith(command, "run")) {
        commands.push_back({ExecutionCommandType::kAbsoluteLimit,
                            std::numeric_limits<int>::max()});
      } else if (base::StartsWith(command, "next")) {
        commands.push_back(
            {ExecutionCommandType::kRelativeLimit, GetParamOr(1)});
      } else if (base::StartsWith(command, "skip")) {
        commands.push_back({ExecutionCommandType::kSkipAction, GetParamOr(1)});
      } else if (base::StartsWith(command, "show")) {
        commands.push_back({ExecutionCommandType::kShowAction, GetParamOr(1)});
      } else if (base::StartsWith(command, "where")) {
        commands.push_back({ExecutionCommandType::kWhereAmI});
      } else if (base::StartsWith(command, "help")) {
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
};

// Blockingly reads the commands from |command_file_path| and executes them.
// Execution primarily means manipulation of the |execution_state|, particularly
// `execution_state.limit`.
ExecutionState ProcessCommands(ExecutionState execution_state,
                               const base::Value::ListView* action_list,
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
            LOG(INFO) << "Action " << (i - execution_state.index) << ": "
                      << (*action_list)[i].DebugString();
          }
          break;
        }
        case ExecutionCommandType::kWhereAmI: {
          LOG(INFO) << "Next action is at position " << execution_state.index
                    << ", limit (excl) is at " << execution_state.limit
                    << ", last (excl) is at " << execution_state.length;
        }
      }
    }
  }
  return execution_state;
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
  {
    if (!base::ReadFileToString(config_file_path, &json_text)) {
      LOG(WARNING) << "Could not read json file: " << config_file_path;
      return sites;
    }
  }
  // Parse json text content to json value node.
  base::Value root_node;
  {
    JSONReader::ValueWithError value_with_error =
        JSONReader::ReadAndReturnValueWithError(
            json_text, JSONParserOptions::JSON_PARSE_RFC);
    if (!value_with_error.value) {
      LOG(WARNING) << "Could not load test config from json file: "
                   << "`testcases.json` because: "
                   << value_with_error.error_message;
      return sites;
    }
    root_node = std::move(value_with_error.value.value());
  }
  base::Value* list_node = root_node.FindListKey("tests");
  if (!list_node) {
    LOG(WARNING) << "No tests found in `testcases.json` config";
    return sites;
  }

  bool also_run_disabled = testing::FLAGS_gtest_also_run_disabled_tests == 1;
  for (auto& item : list_node->GetList()) {
    const base::DictionaryValue* dict;
    if (!item.GetAsDictionary(&dict))
      continue;
    CapturedSiteParams param;
    param.site_name = *(dict->FindStringKey("site_name"));
    if (dict->HasKey("scenario_dir"))
      param.scenario_dir = *(dict->FindStringKey("scenario_dir"));
    param.is_disabled = dict->FindBoolKey("disabled").value_or(false);
    if (param.is_disabled && !also_run_disabled)
      continue;

    const std::string* expectation_string = dict->FindStringKey("expectation");
    if (expectation_string && *expectation_string == "FAIL") {
      param.expectation = kFail;
    } else {
      param.expectation = kPass;
    }
    // Check that a pair of .test and .wpr files exist - otherwise skip
    base::FilePath file_name = replay_files_dir_path;
    if (!param.scenario_dir.empty())
      file_name = file_name.AppendASCII(param.scenario_dir);
    file_name = file_name.AppendASCII(param.site_name);

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
    sites.push_back(param);
  }
  return sites;
}

std::string FilePathToUTF8(const base::FilePath::StringType& str) {
#if defined(OS_WIN)
  return base::WideToUTF8(str);
#else
  return str;
#endif
}

base::Optional<base::FilePath> GetCommandFilePath() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line && command_line->HasSwitch(kCommandFileFlag)) {
    return base::make_optional(
        command_line->GetSwitchValuePath(kCommandFileFlag));
  }
  return base::nullopt;
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
  LOG(INFO) << base::StringPrintf(msg, test_file_name, kCommandFileFlag);
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
  content::RenderFrameHost* frame = FrameMatchingPredicate(
      web_contents(), base::BindRepeating(&content::FrameMatchesName, name));
  if (frame) {
    return frame;
  } else {
    query_type_ = NAME;
    frame_name_ = name;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop_.QuitClosure(), timeout);
    run_loop_.Run();
    return target_frame_;
  }
}

content::RenderFrameHost* IFrameWaiter::WaitForFrameMatchingOrigin(
    const GURL origin,
    const base::TimeDelta timeout) {
  content::RenderFrameHost* frame = FrameMatchingPredicate(
      web_contents(), base::BindRepeating(&FrameHasOrigin, origin));
  if (frame) {
    return frame;
  } else {
    query_type_ = ORIGIN;
    origin_ = origin;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop_.QuitClosure(), timeout);
    run_loop_.Run();
    return target_frame_;
  }
}

content::RenderFrameHost* IFrameWaiter::WaitForFrameMatchingUrl(
    const GURL url,
    const base::TimeDelta timeout) {
  content::RenderFrameHost* frame = FrameMatchingPredicate(
      web_contents(), base::BindRepeating(&content::FrameHasSourceUrl, url));
  if (frame) {
    return frame;
  } else {
    query_type_ = URL;
    url_ = url;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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
      if (render_frame_host->GetLastCommittedURL().GetOrigin() == origin_)
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
      if (validated_url.GetOrigin() == origin_)
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
  return (url.GetOrigin() == origin.GetOrigin());
}

// TestRecipeReplayer ---------------------------------------------------------
TestRecipeReplayer::TestRecipeReplayer(
    Browser* browser,
    TestRecipeReplayChromeFeatureActionExecutor* feature_action_executor)
    : browser_(browser), feature_action_executor_(feature_action_executor) {}

TestRecipeReplayer::~TestRecipeReplayer() {}

bool TestRecipeReplayer::ReplayTest(
    const base::FilePath& capture_file_path,
    const base::FilePath& recipe_file_path,
    const base::Optional<base::FilePath>& command_file_path) {
  if (!StartWebPageReplayServer(capture_file_path))
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
  base::Optional<base::Value> parsed_json =
      base::JSONReader::Read(decompressed_json_text);
  if (!parsed_json) {
    VLOG(1) << kClockNotSetMessage << "Failed to deserialize json";
    return false;
  }
  std::unique_ptr<base::DictionaryValue> wpr_info = base::DictionaryValue::From(
      base::Value::ToUniquePtrValue(std::move(*parsed_json)));

  base::Value* time_value = wpr_info->FindKey("DeterministicTimeSeedMs");
  if (!time_value) {
    VLOG(1) << kClockNotSetMessage << "No DeterministicTimeSeedMs found";
    return false;
  }
  // wpr archive stores time seed in ms, clock is set in seconds.
  test_clock_.SetNow(base::Time::FromDoubleT(time_value->GetDouble() / 1000));
  return true;
}

// static
void TestRecipeReplayer::SetUpCommandLine(base::CommandLine* command_line) {
  // Direct traffic to the Web Page Replay server.
  command_line->AppendSwitchASCII(
      network::switches::kHostResolverRules,
      base::StringPrintf(
          "MAP *:80 127.0.0.1:%d,"
          "MAP *:443 127.0.0.1:%d,"
          // Set to always exclude, allows cache_replayer overwrite
          "EXCLUDE clients1.google.com,"
          "EXCLUDE localhost",
          kHostHttpPort, kHostHttpsPort));
  command_line->AppendSwitchASCII(
      network::switches::kIgnoreCertificateErrorsSPKIList,
      kWebPageReplayCertSPKI);
  command_line->AppendSwitch(switches::kStartMaximized);
}

void TestRecipeReplayer::Setup() {
  CleanupSiteData();

  // Bypass permission dialogs.
  permissions::PermissionRequestManager::FromWebContents(GetWebContents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::ACCEPT_ALL);
}

void TestRecipeReplayer::Cleanup() {
  // If there are still cookies at the time the browser test shuts down,
  // Chrome's SQL lite persistent cookie store will crash.
  CleanupSiteData();
  EXPECT_TRUE(StopWebPageReplayServer())
      << "Cannot stop the local Web Page Replay server.";
}

TestRecipeReplayChromeFeatureActionExecutor*
TestRecipeReplayer::feature_action_executor() {
  return feature_action_executor_;
}

Browser* TestRecipeReplayer::browser() {
  return browser_;
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
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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
  ui_test_utils::NavigateToURL(browser_, GURL(url::kAboutBlankURL));
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(browser_->profile());
  content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_COOKIES,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      &completion_observer);
  completion_observer.BlockUntilCompletion();
}

bool TestRecipeReplayer::StartWebPageReplayServer(
    const base::FilePath& capture_file_path) {
  std::vector<std::string> args;
  base::FilePath src_dir;
  if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir)) {
    ADD_FAILURE() << "Failed to extract the Chromium source directory!";
    return false;
  }

  args.push_back(base::StringPrintf("--http_port=%d", kHostHttpPort));
  args.push_back(base::StringPrintf("--https_port=%d", kHostHttpsPort));
  args.push_back("--serve_response_in_chronological_sequence");
  // Start WPR in quiet mode, removing the extra verbose ServeHTTP interactions
  // that are for the the overwhelming majority unhelpful, but for extra
  // debugging of a test case, this might make sense to comment out.
  args.push_back("--quiet_mode");
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
  if (!RunWebPageReplayCmd("replay", args, &web_page_replay_server_))
    return false;

  // Sleep 5 seconds to wait for the web page replay server to start.
  // TODO(crbug.com/847910): create a process std stream reader class to use the
  // process output to determine when the server is ready
  base::RunLoop wpr_launch_waiter;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, wpr_launch_waiter.QuitClosure(),
      base::TimeDelta::FromSeconds(5));
  wpr_launch_waiter.Run();

  if (!web_page_replay_server_.IsValid()) {
    ADD_FAILURE() << "Failed to start the WPR replay server!";
    return false;
  }

  return true;
}

bool TestRecipeReplayer::StopWebPageReplayServer() {
  if (web_page_replay_server_.IsValid()) {
    if (!web_page_replay_server_.Terminate(0, true)) {
      ADD_FAILURE() << "Failed to terminate the WPR replay server!";
      return false;
    }
  }

  // The test server hasn't started, no op.
  return true;
}

bool TestRecipeReplayer::RunWebPageReplayCmdAndWaitForExit(
    const std::string& cmd,
    const std::vector<std::string>& args,
    const base::TimeDelta& timeout) {
  base::Process process;
  int exit_code;

  if (RunWebPageReplayCmd(cmd, args, &process) && process.IsValid() &&
      process.WaitForExitWithTimeout(timeout, &exit_code) && exit_code == 0) {
    return true;
  }

  ADD_FAILURE() << "Failed to run WPR command: '" << cmd << "'!";
  return false;
}

bool TestRecipeReplayer::RunWebPageReplayCmd(
    const std::string& cmd,
    const std::vector<std::string>& args,
    base::Process* process) {
  // Allow the function to block. Otherwise the subsequent call to
  // base::PathExists will fail. base::PathExists must be called from
  // a scope that allows blocking.
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::LaunchOptions options = base::LaunchOptionsForTest();
  base::FilePath exe_dir;
  if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &exe_dir)) {
    ADD_FAILURE() << "Failed to extract the Chromium source directory!";
    return false;
  }

  base::FilePath web_page_replay_binary_dir = exe_dir.AppendASCII("third_party")
                                                  .AppendASCII("catapult")
                                                  .AppendASCII("telemetry")
                                                  .AppendASCII("telemetry")
                                                  .AppendASCII("bin");
  options.current_directory = web_page_replay_binary_dir;

#if defined(OS_WIN)
  base::FilePath wpr_executable_binary =
      base::FilePath(FILE_PATH_LITERAL("win"))
          .AppendASCII("AMD64")
          .AppendASCII("wpr.exe");
#elif defined(OS_MAC)
  base::FilePath wpr_executable_binary =
      base::FilePath(FILE_PATH_LITERAL("mac"))
          .AppendASCII("x86_64")
          .AppendASCII("wpr");
#elif defined(OS_POSIX)
  base::FilePath wpr_executable_binary =
      base::FilePath(FILE_PATH_LITERAL("linux"))
          .AppendASCII("x86_64")
          .AppendASCII("wpr");
#else
#error Plaform is not supported.
#endif
  base::CommandLine full_command(
      web_page_replay_binary_dir.Append(wpr_executable_binary));
  full_command.AppendArg(cmd);

  // Ask web page replay to use the custom certificate and key files used to
  // make the web page captures.
  // The capture files used in these browser tests are also used on iOS to
  // test autofill.
  // The custom cert and key files are different from those of the offical
  // WPR releases. The custom files are made to work on iOS.
  base::FilePath src_dir;
  if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir)) {
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
      "--https_cert_file=%s",
      FilePathToUTF8(
          web_page_replay_support_file_dir.AppendASCII("wpr_cert.pem").value())
          .c_str()));
  full_command.AppendArg(base::StringPrintf(
      "--https_key_file=%s",
      FilePathToUTF8(
          web_page_replay_support_file_dir.AppendASCII("wpr_key.pem").value())
          .c_str()));

  for (const auto& arg : args)
    full_command.AppendArg(arg);

  LOG(INFO) << full_command.GetArgumentsString();

  *process = base::LaunchProcess(full_command, options);
  return true;
}

const std::string* FindPopulateString(
                        const base::DictionaryValue& container,
                        const std::string key_name,
                        const std::string key_descriptor) {
  const std::string* string_value = container.FindStringKey(key_name);
  if (!string_value) {
    ADD_FAILURE() << "Failed to extract '" << key_descriptor
                  << "' from container!";
    return nullptr;
  }
  return string_value;
}

bool TestRecipeReplayer::ReplayRecordedActions(
    const base::FilePath& recipe_file_path,
    const base::Optional<base::FilePath>& command_file_path) {
  // Read the text of the recipe file.
  base::ScopedAllowBlockingForTesting for_testing;
  std::string json_text;
  if (!base::ReadFileToString(recipe_file_path, &json_text)) {
    ADD_FAILURE() << "Failed to read recipe file '" << recipe_file_path << "'!";
    return false;
  }

  // Convert the file text into a json object.
  base::Optional<base::Value> parsed_json = base::JSONReader::Read(json_text);
  if (!parsed_json) {
    ADD_FAILURE() << "Failed to deserialize json text!";
    return false;
  }
  std::unique_ptr<base::DictionaryValue> recipe = base::DictionaryValue::From(
      base::Value::ToUniquePtrValue(std::move(*parsed_json)));

  if (!InitializeBrowserToExecuteRecipe(recipe))
    return false;

  // Iterate through and execute each action in the recipe.
  base::Value* action_list_container = recipe->FindListKey("actions");
  if (!action_list_container) {
    ADD_FAILURE() << "Failed to extract action list from the recipe!";
    return false;
  }

  auto action_list = action_list_container->GetList();
  ExecutionState execution_state{.length =
                                     static_cast<int>(action_list.size())};
  if (command_file_path.has_value()) {
    execution_state.limit = 0;  // Stop execution initially in debug mode.
    PrintDebugInstructions(command_file_path.value());
  }

  while (execution_state.index < execution_state.length) {
    if (command_file_path.has_value()) {
      while (execution_state.limit <= execution_state.index) {
        bool thread_finished = false;
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, {base::MayBlock()},
            base::BindOnce(&ProcessCommands, execution_state, &action_list,
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
          base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
              FROM_HERE, run_loop.QuitClosure(),
              base::TimeDelta::FromMilliseconds(1000));
          run_loop.Run();
        }
      }
    }
    LOG(INFO) << "Proceeding with execution with action "
              << execution_state.index << " of " << execution_state.length
              << ": " << action_list[execution_state.index];

    base::DictionaryValue* action;
    if (!action_list[execution_state.index].GetAsDictionary(&action)) {
      ADD_FAILURE()
          << "Failed to extract an individual action from the recipe!";
      return false;
    }
    const std::string* type =
        FindPopulateString(*action, "type", "action type");
    if (!type)
      return false;
    if (base::CompareCaseInsensitiveASCII(*type, "autofill") == 0) {
      if (!ExecuteAutofillAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "click") == 0) {
      if (!ExecuteClickAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "clickIfNotSeen") ==
               0) {
      if (!ExecuteClickIfNotSeenAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "closeTab") == 0) {
      if (!ExecuteCloseTabAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "coolOff") == 0) {
      if (!ExecuteCoolOffAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "executeScript") == 0) {
      if (!ExecuteRunCommandAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "hover") == 0) {
      if (!ExecuteHoverAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "loadPage") == 0) {
      if (!ExecuteForceLoadPage(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "pressEnter") == 0) {
      if (!ExecutePressEnterAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "pressEscape") == 0) {
      if (!ExecutePressEscapeAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "pressSpace") == 0) {
      if (!ExecutePressSpaceAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "savePassword") == 0) {
      if (!ExecuteSavePasswordAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "select") == 0) {
      if (!ExecuteSelectDropdownAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "type") == 0) {
      if (!ExecuteTypeAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "typePassword") == 0) {
      if (!ExecuteTypePasswordAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "updatePassword") ==
               0) {
      if (!ExecuteUpdatePasswordAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "validateField") == 0) {
      if (!ExecuteValidateFieldValueAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(
                   *type, "validateNoSavePasswordPrompt") == 0) {
      if (!ExecuteValidateNoSavePasswordPromptAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(
                   *type, "validatePasswordSaveFallback") == 0) {
      if (!ExecuteValidateSaveFallbackAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(*type, "waitFor") == 0) {
      if (!ExecuteWaitForStateAction(*action))
        return false;
    } else {
      ADD_FAILURE() << "Unrecognized action type: " << *type;
    }

    ++execution_state.index;
  }

  // Dismiss the beforeUnloadDialog if the last page of the test has a
  // beforeUnload function.
  if (recipe->FindKey("dismissBeforeUnload")) {
    NavigateAwayAndDismissBeforeUnloadDialog();
  }

  return true;
}

// Functions for deserializing and executing actions from the test recipe
// JSON object.
bool TestRecipeReplayer::InitializeBrowserToExecuteRecipe(
    const std::unique_ptr<base::DictionaryValue>& recipe) {
  // Setup any saved address and credit card at the start of the test.
  const base::Value* autofill_profile_container =
      recipe->FindKey("autofillProfile");

  if (autofill_profile_container &&
      !SetupSavedAutofillProfile(*autofill_profile_container))
    return false;

  // Setup any saved passwords at the start of the test.
  const base::Value* saved_password_container =
      recipe->FindKey("passwordManagerProfiles");

  if (saved_password_container &&
      !SetupSavedPasswords(*saved_password_container))
    return false;

  // Extract the starting URL from the test recipe.
  const std::string* starting_url = recipe->FindStringKey("startingURL");
  if (!starting_url) {
    ADD_FAILURE() << "Failed to extract the starting url from the recipe!";
    return false;
  }

  // Navigate to the starting URL, wait for the page to complete loading.
  if (!content::ExecuteScript(GetWebContents(),
                              base::StringPrintf("window.location.href = '%s';",
                                                 starting_url->c_str()))) {
    ADD_FAILURE() << "Failed to navigate Chrome to '" << starting_url << "!";
    return false;
  }

  WaitTillPageIsIdle();
  return true;
}

bool TestRecipeReplayer::ExecuteAutofillAction(
    const base::DictionaryValue& action) {
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

  if (!feature_action_executor()->AutofillForm(
          xpath, frame_path, kAutofillActionNumRetries, frame))
    return false;
  WaitTillPageIsIdle(kAutofillActionWaitForVisualUpdateTimeout);
  return true;
}

bool TestRecipeReplayer::ExecuteClickAction(
    const base::DictionaryValue& action) {
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

bool TestRecipeReplayer::ExecuteClickIfNotSeenAction(
    const base::DictionaryValue& action) {
  std::string xpath;
  content::RenderFrameHost* frame;
  if (ExtractFrameAndVerifyElement(action, &xpath, &frame, false, false,
                                   true)) {
    return true;
  } else {
    const std::string* click_xpath_text =
        FindPopulateString(action, "clickSelector", "click xpath selector");
    base::Value click_action = action.Clone();
    click_action.SetStringKey("selector", *click_xpath_text);
    return ExecuteClickAction(base::Value::AsDictionaryValue(click_action));
  }
}

bool TestRecipeReplayer::ExecuteCloseTabAction(
    const base::DictionaryValue& action) {
  VLOG(1) << "Closing Active Tab";
  browser_->tab_strip_model()->CloseSelectedTabs();
  return true;
}

bool TestRecipeReplayer::ExecuteCoolOffAction(
    const base::DictionaryValue& action) {
  base::RunLoop heart_beat;
  base::TimeDelta cool_off_time = cool_off_action_timeout;
  const base::Value* pause_time_container = action.FindKey("pauseTimeSec");
  if (pause_time_container) {
    if (!pause_time_container->is_int()) {
      ADD_FAILURE() << "Pause time is not an integer!";
      return false;
    }
    int seconds = pause_time_container->GetInt();
    cool_off_time = base::TimeDelta::FromSeconds(seconds);
  }
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, heart_beat.QuitClosure(), cool_off_time);
  VLOG(1) << "Pausing execution for '" << cool_off_time.InSeconds()
          << "' seconds";
  heart_beat.Run();

  return true;
}

bool TestRecipeReplayer::ExecuteHoverAction(
    const base::DictionaryValue& action) {
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

bool TestRecipeReplayer::ExecuteForceLoadPage(
    const base::DictionaryValue& action) {
  bool shouldForce = action.FindBoolKey("force").value_or(false);
  if (!shouldForce)
    return true;

  const std::string* url = FindPopulateString(action, "url", "Force Load URL");
  if (!url)
    return false;
  VLOG(1) << "Making explicit URL redirect to '" << *url << "'";
  ui_test_utils::NavigateToURL(browser_, GURL(*url));

  WaitTillPageIsIdle();

  return true;
}

bool TestRecipeReplayer::ExecutePressEnterAction(
    const base::DictionaryValue& action) {
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

bool TestRecipeReplayer::ExecutePressEscapeAction(
    const base::DictionaryValue& action) {
  VLOG(1) << "Pressing 'Esc' in the current frame";
  SimulateKeyPressWrapper(GetWebContents(), ui::DomKey::ESCAPE);
  WaitTillPageIsIdle();
  return true;
}

bool TestRecipeReplayer::ExecutePressSpaceAction(
    const base::DictionaryValue& action) {
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

bool TestRecipeReplayer::ExecuteRunCommandAction(
    const base::DictionaryValue& action) {
  // Extract the list of JavaScript commands into a vector.
  std::vector<std::string> commands;

  const base::Value* list_container = action.FindListKey("commands");
  if (!list_container) {
    ADD_FAILURE() << "Failed to extract commands list from action";
    return false;
  }
  for (const auto& command : list_container->GetList()) {
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
    if (!content::ExecuteScript(frame, command)) {
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

bool TestRecipeReplayer::ExecuteSavePasswordAction(
    const base::DictionaryValue& action) {
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

bool TestRecipeReplayer::ExecuteSelectDropdownAction(
    const base::DictionaryValue& action) {
  base::Optional<int> index = action.FindIntKey("index");
  if (!index) {
    ADD_FAILURE() << "Failed to extract Selection Index from action";
    return false;
  }

  std::string xpath;
  content::RenderFrameHost* frame;
  if (!ExtractFrameAndVerifyElement(action, &xpath, &frame))
    return false;

  VLOG(1) << "Select option '" << index.value() << "' from `" << xpath << "`.";
  if (!ExecuteJavaScriptOnElementByXpath(
          frame, xpath,
          base::StringPrintf(
              "automation_helper"
              "  .selectOptionFromDropDownElementByIndex(target, %d);",
              index.value()))) {
    ADD_FAILURE() << "Failed to select drop down option with JavaScript!";
    return false;
  }
  WaitTillPageIsIdle();
  return true;
}

bool TestRecipeReplayer::ExecuteTypeAction(
    const base::DictionaryValue& action) {
  const std::string* value =
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

bool TestRecipeReplayer::ExecuteTypePasswordAction(
    const base::DictionaryValue& action) {
  std::string xpath;
  content::RenderFrameHost* frame;
  if (!ExtractFrameAndVerifyElement(action, &xpath, &frame, true))
    return false;

  const std::string* value =
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

bool TestRecipeReplayer::ExecuteUpdatePasswordAction(
    const base::DictionaryValue& action) {
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
    const base::DictionaryValue& action) {
  std::string xpath;
  content::RenderFrameHost* frame;
  if (!ExtractFrameAndVerifyElement(action, &xpath, &frame, false, true))
    return false;

  const base::Value* autofill_prediction_container =
      action.FindKey("expectedAutofillType");
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
    ExpectElementPropertyEquals(
        frame, xpath,
        "return target.getAttribute('autofill-prediction');",
        expected_autofill_prediction_type, "autofill type mismatch", true);
  }

  const std::string* expected_value = FindPopulateString(action,
    "expectedValue", "validation expected value");
  if (!expected_value)
    return false;

  VLOG(1) << "Checking the field `" << xpath << "`.";
  ExpectElementPropertyEquals(frame, xpath, "return target.value;",
                              *expected_value, "text value mismatch");
  return true;
}

bool TestRecipeReplayer::ExecuteValidateNoSavePasswordPromptAction(
    const base::DictionaryValue& action) {
  VLOG(1) << "Verify that the page hasn't shown a save password prompt.";
  EXPECT_FALSE(feature_action_executor()->HasChromeShownSavePasswordPrompt());
  return true;
}

bool TestRecipeReplayer::ExecuteValidateSaveFallbackAction(
    const base::DictionaryValue& action) {
  VLOG(1) << "Verify that Chrome shows the save fallback icon in the omnibox.";
  EXPECT_TRUE(feature_action_executor()->WaitForSaveFallback());
  return true;
}

bool TestRecipeReplayer::ExecuteWaitForStateAction(
    const base::DictionaryValue& action) {
  // Extract the list of JavaScript assertions into a vector.
  std::vector<std::string> state_assertions;

  const base::Value* list_container = action.FindListKey("assertions");
  if (!list_container) {
    ADD_FAILURE() << "Failed to extract wait assertions list from action";
    return false;
  }
  for (const base::Value& assertion : list_container->GetList()) {
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
    const base::DictionaryValue& action,
    std::string* xpath) {
  xpath->clear();
  const std::string* xpath_text =
      FindPopulateString(action, "selector", "xpath selector");
  if (!xpath_text)
    return false;
  *xpath = *xpath_text;
  return true;
}

bool TestRecipeReplayer::GetTargetHTMLElementVisibilityEnumFromAction(
    const base::DictionaryValue& action,
    int* visibility_enum_val) {
  const base::Value* visibility_container = action.FindKey("visibility");
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
    const base::DictionaryValue& action,
    content::RenderFrameHost** frame) {
  const base::Value* iframe_container = action.FindKey("context");
  if (!iframe_container) {
    ADD_FAILURE() << "Failed to extract the iframe context from action!";
    return false;
  }

  const base::DictionaryValue* iframe;
  if (!iframe_container->GetAsDictionary(&iframe)) {
    ADD_FAILURE() << "Failed to extract the iframe context object!";
    return false;
  }

  base::Optional<bool> is_iframe_container = iframe->FindBoolKey("isIframe");
  if (!is_iframe_container) {
    ADD_FAILURE() << "Failed to extract isIframe from the iframe context! ";
    return false;
  }
  if (!is_iframe_container.value_or(false)) {
    *frame = GetWebContents()->GetMainFrame();
    return true;
  }

  const base::Value* frame_name_container =
      iframe->FindPath({"browserTest", "name"});
  const base::Value* frame_origin_container =
      iframe->FindPath({"browserTest", "origin"});
  const base::Value* frame_url_container =
      iframe->FindPath({"browserTest", "url"});
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

  if (frame == nullptr) {
    ADD_FAILURE() << "Failed to find iframe!";
    return false;
  }

  return true;
}

bool TestRecipeReplayer::ExtractFrameAndVerifyElement(
    const base::DictionaryValue& action,
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
    const base::DictionaryValue& action,
    std::vector<std::string>* iframe_path) {
  *iframe_path = std::vector<std::string>();

  const base::Value* iframe_container = action.FindKey("context");
  if (!iframe_container) {
    ADD_FAILURE() << "Failed to extract the iframe context from action!";
    return false;
  }

  const base::DictionaryValue* iframe;
  if (!iframe_container->GetAsDictionary(&iframe)) {
    ADD_FAILURE() << "Failed to extract the iframe context object!";
    return false;
  }

  const base::Value* iframe_path_container = iframe->FindKey("path");
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
    bool assertion_passed = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        frame,
        base::StringPrintf("window.domAutomationController.send("
                           "    (function() {"
                           "      try {"
                           "        %s"
                           "      } catch (ex) {}"
                           "      return false;"
                           "    })());",
                           assertion.c_str()),
        &assertion_passed));
    if (!assertion_passed) {
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
  return ExecuteScript(frame, js);
}

bool TestRecipeReplayer::GetElementProperty(
    const content::ToRenderFrameHost& frame,
    const std::string& element_xpath,
    const std::string& get_property_function_body,
    std::string* property) {
  return ExecuteScriptAndExtractString(
      frame,
      base::StringPrintf(
          "window.domAutomationController.send("
          "    (function() {"
          "      try {"
          "        var element = function() {"
          "          return automation_helper.getElementByXpath(`%s`);"
          "        }();"
          "        return function(target){%s}(element);"
          "      } catch (ex) {}"
          "      return 'Exception encountered';"
          "    })());",
          element_xpath.c_str(), get_property_function_body.c_str()),
      property);
}

bool TestRecipeReplayer::ExpectElementPropertyEquals(
    const content::ToRenderFrameHost& frame,
    const std::string& element_xpath,
    const std::string& get_property_function_body,
    const std::string& expected_value,
    const std::string& validation_field,
    bool ignore_case) {
  std::string value;
  if (!GetElementProperty(frame, element_xpath, get_property_function_body,
                          &value)) {
    ADD_FAILURE() << "Failed to extract element property! " << element_xpath
                  << ", " << get_property_function_body;
    return false;
  }

  if ((ignore_case &&
       !base::EqualsCaseInsensitiveASCII(expected_value, value)) ||
      (!ignore_case && expected_value != value)) {
    std::string error_message = base::StrCat(
      {"Field xpath: `", element_xpath, "` ", validation_field, ", ",
       "Expected: '" , expected_value, "', actual: '", value, "'"});
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
      "  window.domAutomationController.send(true);"
      "} catch(ex) {"
      "  window.domAutomationController.send(false);"
      "}",
      element_xpath.c_str()));

  bool succeeded = false;
  if (!ExecuteScriptAndExtractBool(frame, scroll_target_js, &succeeded)) {
    ADD_FAILURE() << "Failed to scroll the element into view with JavaScript!";
    return false;
  }
  return true;
}

bool TestRecipeReplayer::PlaceFocusOnElement(
    const std::string& element_xpath,
    const std::vector<std::string> iframe_path,
    content::RenderFrameHost* frame) {
  if (!ScrollElementIntoView(element_xpath, frame))
    return false;

  const std::string focus_on_target_field_js(base::StringPrintf(
      "try {"
      "  function onFocusHandler(event) {"
      "    event.target.removeEventListener(event.type, arguments.callee);"
      "    window.domAutomationController.send(true);"
      "  }"
      "  const element = automation_helper.getElementByXpath(`%s`);"
      "  if (document.activeElement === element) {"
      "    window.domAutomationController.send(true);"
      "  } else {"
      "    element.addEventListener('focus', onFocusHandler);"
      "    element.focus();"
      "  }"
      "  setTimeout(() => {"
      "    element.removeEventListener('focus', onFocusHandler);"
      "    window.domAutomationController.send(false);"
      "  }, 1000);"
      "} catch(ex) {"
      "  window.domAutomationController.send(false);"
      "}",
      element_xpath.c_str()));

  bool focused = false;
  if (!ExecuteScriptAndExtractBool(frame, focus_on_target_field_js, &focused)) {
    ADD_FAILURE() << "Failed to place focus on the element with JavaScript!";
    return false;
  }

  if (focused) {
    return true;
  } else {
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
  std::string rect_str;
  const std::string get_element_bounding_rect_js(base::StringPrintf(
      "window.domAutomationController.send("
      "    (function() {"
      "       try {"
      "         const element = automation_helper.getElementByXpath(`%s`);"
      "         const rect = element.getBoundingClientRect();"
      "         return Math.round(rect.left) + ',' + "
      "                Math.round(rect.top) + ',' + "
      "                Math.round(rect.width) + ',' + "
      "                Math.round(rect.height);"
      "       } catch(ex) {}"
      "       return '';"
      "    })());",
      target_element_xpath.c_str()));

  if (!content::ExecuteScriptAndExtractString(
          frame, get_element_bounding_rect_js, &rect_str)) {
    ADD_FAILURE()
        << "Failed to run script to extract target element's bounding rect!";
    return false;
  }

  if (rect_str.empty()) {
    ADD_FAILURE() << "Failed to extract target element's bounding rect!";
    return false;
  }

  // Parse the bounding rect string to extract the element coordinates.
  std::istringstream rect_stream(rect_str);
  std::string token;
  if (!std::getline(rect_stream, token, ',')) {
    ADD_FAILURE() << "Failed to extract target element's x coordinate from "
                  << "the string `" << rect_str << "`!";
    return false;
  }

  output_rect->set_x(std::stoi(token));

  if (!std::getline(rect_stream, token, ',')) {
    ADD_FAILURE() << "Failed to extract target element's y coordinate from "
                  << "the string `" << rect_str << "`!";
    return false;
  }

  output_rect->set_y(std::stoi(token));

  if (!std::getline(rect_stream, token, ',')) {
    ADD_FAILURE() << "Failed to extract target element's width from "
                  << "the string `" << rect_str << "`!";
    return false;
  }

  output_rect->set_width(std::stoi(token));

  if (!std::getline(rect_stream, token, ',')) {
    ADD_FAILURE() << "Failed to extract target element's height from "
                  << "the string `" << rect_str << "`!";
    return false;
  }

  output_rect->set_height(std::stoi(token));

  return true;
}

bool TestRecipeReplayer::GetBoundingRectOfTargetElement(
    const std::string& target_element_xpath,
    const std::vector<std::string> iframe_path,
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

void TestRecipeReplayer::NavigateAwayAndDismissBeforeUnloadDialog() {
  content::PrepContentsForBeforeUnloadTest(GetWebContents());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  javascript_dialogs::AppModalDialogController* alert =
      ui_test_utils::WaitForAppModalDialog();
  alert->view()->AcceptAppModalDialog();
}

bool TestRecipeReplayer::HasChromeStoredCredential(
    const base::DictionaryValue& action,
    bool* stored_cred) {
  const std::string* origin = FindPopulateString(action, "origin", "Origin");
  const std::string* username = FindPopulateString(action,
                                                   "userName", "Username");
  const std::string* password = FindPopulateString(action,
                                                   "password", "Password");
  if (!origin || !username || !password)
    return false;
  *stored_cred = feature_action_executor()->HasChromeStoredCredential(
      *origin, *username, *password);

  return true;
}

bool TestRecipeReplayer::SetupSavedAutofillProfile(
    const base::Value& saved_autofill_profile_container) {
  if (!saved_autofill_profile_container.is_list()) {
    ADD_FAILURE() << "Save Autofill Profile is not a list!";
    return false;
  }

  for (const auto& list_entry : saved_autofill_profile_container.GetList()) {
    const base::DictionaryValue* entry;
    if (!list_entry.GetAsDictionary(&entry)) {
      ADD_FAILURE() << "Failed to extract an entry!";
      return false;
    }

    const std::string* type =
        FindPopulateString(*entry, "type", "profile field type");
    const std::string* value =
        FindPopulateString(*entry, "value", "profile field value");
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
  if (saved_autofill_profile_container.GetList().empty()) {
    return true;
  }

  return feature_action_executor()->SetupAutofillProfile();
}

bool TestRecipeReplayer::SetupSavedPasswords(
    const base::Value& saved_password_list_container) {
  if (!saved_password_list_container.is_list()) {
    ADD_FAILURE() << "Saved Password List is not a list!";
    return false;
  }

  for (const auto& entry : saved_password_list_container.GetList()) {
    const base::DictionaryValue* cred;
    if (!entry.GetAsDictionary(&cred)) {
      ADD_FAILURE() << "Failed to extract a saved password!";
      return false;
    }

    const std::string* origin =
        FindPopulateString(*cred, "website", "Website");
    const std::string* username =
        FindPopulateString(*cred, "username", "Username");
    const std::string* password =
        FindPopulateString(*cred, "password", "Password");
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
    content::RenderFrameHost* frame) {
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
