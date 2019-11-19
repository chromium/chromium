// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/lazy_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_switches.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/installer/util/shell_util.h"
#endif

#if !defined(OS_WIN)
#include "chrome/common/channel_info.h"
#include "chrome/grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

using content::BrowserThread;

namespace shell_integration {

namespace {

const struct AppModeInfo* gAppModeInfo = nullptr;

// TODO(crbug.com/773563): Remove |g_sequenced_task_runner| and use an instance
// field / singleton instead.
#if defined(OS_WIN)
base::LazyCOMSTATaskRunner g_sequenced_task_runner =
    LAZY_COM_STA_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::ThreadPool(), base::MayBlock()),
        base::SingleThreadTaskRunnerThreadMode::SHARED);
#else
base::LazySequencedTaskRunner g_sequenced_task_runner =
    LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::ThreadPool(), base::MayBlock()));
#endif

}  // namespace

bool CanSetAsDefaultBrowser() {
  return GetDefaultWebClientSetPermission() != SET_DEFAULT_NOT_ALLOWED;
}

#if !defined(OS_WIN)
bool IsElevationNeededForSettingDefaultProtocolClient() {
  return false;
}
#endif  // !defined(OS_WIN)

void SetAppModeInfo(const struct AppModeInfo* info) {
  gAppModeInfo = info;
}

const struct AppModeInfo* AppModeInfo() {
  return gAppModeInfo;
}

bool IsRunningInAppMode() {
  return gAppModeInfo != NULL;
}

base::CommandLine CommandLineArgsForLauncher(
    const GURL& url,
    const std::string& extension_app_id,
    const base::FilePath& profile_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::CommandLine new_cmd_line(base::CommandLine::NO_PROGRAM);

  AppendProfileArgs(
      extension_app_id.empty() ? base::FilePath() : profile_path,
      &new_cmd_line);

  // If |extension_app_id| is present, we use the kAppId switch rather than
  // the kApp switch (the launch url will be read from the extension app
  // during launch.
  if (!extension_app_id.empty()) {
    new_cmd_line.AppendSwitchASCII(switches::kAppId, extension_app_id);
  } else {
    // Use '--app=url' instead of just 'url' to launch the browser with minimal
    // chrome.
    // Note: Do not change this flag!  Old Gears shortcuts will break if you do!
    new_cmd_line.AppendSwitchASCII(switches::kApp, url.spec());
  }
  return new_cmd_line;
}

void AppendProfileArgs(const base::FilePath& profile_path,
                       base::CommandLine* command_line) {
  DCHECK(command_line);
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();

  // Use the same UserDataDir for new launches that we currently have set.
  base::FilePath user_data_dir =
      cmd_line.GetSwitchValuePath(switches::kUserDataDir);
#if defined(OS_MACOSX) || defined(OS_WIN)
  policy::path_parser::CheckUserDataDirPolicy(&user_data_dir);
#endif
  if (!user_data_dir.empty()) {
    // Make sure user_data_dir is an absolute path.
    user_data_dir = base::MakeAbsoluteFilePath(user_data_dir);
    if (!user_data_dir.empty() && base::PathExists(user_data_dir))
      command_line->AppendSwitchPath(switches::kUserDataDir, user_data_dir);
  }

#if defined(OS_CHROMEOS)
  base::FilePath profile = cmd_line.GetSwitchValuePath(
      chromeos::switches::kLoginProfile);
  if (!profile.empty())
    command_line->AppendSwitchPath(chromeos::switches::kLoginProfile, profile);
#else
  if (!profile_path.empty())
    command_line->AppendSwitchPath(switches::kProfileDirectory,
                                   profile_path.BaseName());
#endif
}

#if !defined(OS_WIN)
base::string16 GetAppShortcutsSubdirName() {
  if (chrome::GetChannel() == version_info::Channel::CANARY)
    return l10n_util::GetStringUTF16(IDS_APP_SHORTCUTS_SUBDIR_NAME_CANARY);
  return l10n_util::GetStringUTF16(IDS_APP_SHORTCUTS_SUBDIR_NAME);
}
#endif  // !defined(OS_WIN)

///////////////////////////////////////////////////////////////////////////////
// DefaultWebClientWorker
//

void DefaultWebClientWorker::StartCheckIsDefault() {
  g_sequenced_task_runner.Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&DefaultWebClientWorker::CheckIsDefault, this, false));
}

void DefaultWebClientWorker::StartSetAsDefault() {
  g_sequenced_task_runner.Get()->PostTask(
      FROM_HERE, base::BindOnce(&DefaultWebClientWorker::SetAsDefault, this));
}

///////////////////////////////////////////////////////////////////////////////
// DefaultWebClientWorker, protected:

DefaultWebClientWorker::DefaultWebClientWorker(
    const DefaultWebClientWorkerCallback& callback,
    const char* worker_name)
    : callback_(callback), worker_name_(worker_name) {}

DefaultWebClientWorker::~DefaultWebClientWorker() = default;

void DefaultWebClientWorker::OnCheckIsDefaultComplete(
    DefaultWebClientState state,
    bool is_following_set_as_default) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  UpdateUI(state);

  if (is_following_set_as_default)
    ReportSetDefaultResult(state);
}

///////////////////////////////////////////////////////////////////////////////
// DefaultWebClientWorker, private:

void DefaultWebClientWorker::CheckIsDefault(bool is_following_set_as_default) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  DefaultWebClientState state = CheckIsDefaultImpl();
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&DefaultBrowserWorker::OnCheckIsDefaultComplete,
                                this, state, is_following_set_as_default));
}

void DefaultWebClientWorker::SetAsDefault() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // SetAsDefaultImpl will make sure the callback is executed exactly once.
  SetAsDefaultImpl(
      base::Bind(&DefaultWebClientWorker::CheckIsDefault, this, true));
}

void DefaultWebClientWorker::ReportSetDefaultResult(
    DefaultWebClientState state) {
  base::LinearHistogram::FactoryGet(
      base::StringPrintf("%s.SetDefaultResult2", worker_name_), 1,
      DefaultWebClientState::NUM_DEFAULT_STATES,
      DefaultWebClientState::NUM_DEFAULT_STATES + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(state);
}

void DefaultWebClientWorker::UpdateUI(DefaultWebClientState state) {
  if (!callback_.is_null()) {
    switch (state) {
      case NOT_DEFAULT:
      case IS_DEFAULT:
      case UNKNOWN_DEFAULT:
      case OTHER_MODE_IS_DEFAULT:
        callback_.Run(state);
        return;
      case NUM_DEFAULT_STATES:
        break;
    }
    NOTREACHED();
  }
}

///////////////////////////////////////////////////////////////////////////////
// DefaultBrowserWorker
//

DefaultBrowserWorker::DefaultBrowserWorker(
    const DefaultWebClientWorkerCallback& callback)
    : DefaultWebClientWorker(callback, "DefaultBrowser") {}

///////////////////////////////////////////////////////////////////////////////
// DefaultBrowserWorker, private:

DefaultBrowserWorker::~DefaultBrowserWorker() = default;

DefaultWebClientState DefaultBrowserWorker::CheckIsDefaultImpl() {
  return GetDefaultBrowser();
}

void DefaultBrowserWorker::SetAsDefaultImpl(
    const base::Closure& on_finished_callback) {
  switch (GetDefaultWebClientSetPermission()) {
    case SET_DEFAULT_NOT_ALLOWED:
      NOTREACHED();
      break;
    case SET_DEFAULT_UNATTENDED:
      SetAsDefaultBrowser();
      break;
    case SET_DEFAULT_INTERACTIVE:
#if defined(OS_WIN)
      if (interactive_permitted_) {
        switch (ShellUtil::GetInteractiveSetDefaultMode()) {
          case ShellUtil::INTENT_PICKER:
            win::SetAsDefaultBrowserUsingIntentPicker();
            break;
          case ShellUtil::SYSTEM_SETTINGS:
            win::SetAsDefaultBrowserUsingSystemSettings(on_finished_callback);
            // Early return because the function above takes care of calling
            // |on_finished_callback|.
            return;
        }
      }
#endif  // defined(OS_WIN)
      break;
  }
  on_finished_callback.Run();
}

///////////////////////////////////////////////////////////////////////////////
// DefaultProtocolClientWorker
//

DefaultProtocolClientWorker::DefaultProtocolClientWorker(
    const DefaultWebClientWorkerCallback& callback,
    const std::string& protocol)
    : DefaultWebClientWorker(callback, "DefaultProtocolClient"),
      protocol_(protocol) {}

///////////////////////////////////////////////////////////////////////////////
// DefaultProtocolClientWorker, protected:

DefaultProtocolClientWorker::~DefaultProtocolClientWorker() = default;

///////////////////////////////////////////////////////////////////////////////
// DefaultProtocolClientWorker, private:

DefaultWebClientState DefaultProtocolClientWorker::CheckIsDefaultImpl() {
  return IsDefaultProtocolClient(protocol_);
}

void DefaultProtocolClientWorker::SetAsDefaultImpl(
    const base::Closure& on_finished_callback) {
  switch (GetDefaultWebClientSetPermission()) {
    case SET_DEFAULT_NOT_ALLOWED:
      // Not allowed, do nothing.
      break;
    case SET_DEFAULT_UNATTENDED:
      SetAsDefaultProtocolClient(protocol_);
      break;
    case SET_DEFAULT_INTERACTIVE:
#if defined(OS_WIN)
      if (interactive_permitted_) {
        switch (ShellUtil::GetInteractiveSetDefaultMode()) {
          case ShellUtil::INTENT_PICKER:
            win::SetAsDefaultProtocolClientUsingIntentPicker(protocol_);
            break;
          case ShellUtil::SYSTEM_SETTINGS:
            win::SetAsDefaultProtocolClientUsingSystemSettings(
                protocol_, on_finished_callback);
            // Early return because the function above takes care of calling
            // |on_finished_callback|.
            return;
        }
      }
#endif  // defined(OS_WIN)
      break;
  }
  on_finished_callback.Run();
}

}  // namespace shell_integration
