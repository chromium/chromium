// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/i18n/file_util_icu.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/installer/util/shell_util.h"
#endif

#if !BUILDFLAG(IS_WIN)
#include "chrome/common/channel_info.h"
#include "chrome/grit/branded_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

using content::BrowserThread;

namespace shell_integration {

namespace {

// TODO(crbug.com/40544199): Remove |g_sequenced_task_runner| and use an
// instance field / singleton instead.
#if BUILDFLAG(IS_WIN)
base::LazyThreadPoolCOMSTATaskRunner g_sequenced_task_runner =
    LAZY_COM_STA_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock()),
        base::SingleThreadTaskRunnerThreadMode::SHARED);
#else
base::LazyThreadPoolSequencedTaskRunner g_sequenced_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock()));
#endif

bool IsValidDefaultWebClientState(DefaultWebClientState state) {
  switch (state) {
    case NOT_DEFAULT:
    case IS_DEFAULT:
    case UNKNOWN_DEFAULT:
    case OTHER_MODE_IS_DEFAULT:
      return true;
    case NUM_DEFAULT_STATES:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

void RunCallback(DefaultWebClientWorkerCallback callback,
                 DefaultWebClientState state) {
  if (!callback.is_null() && IsValidDefaultWebClientState(state)) {
    std::move(callback).Run(state);
    return;
  }
}

DefaultWebClientSetPermission GetDefaultWebClientSetPermission(
    internal::WebClientSetMethod method) {
#if BUILDFLAG(CHROME_FOR_TESTING)
  return SET_DEFAULT_NOT_ALLOWED;
#else
  return internal::GetPlatformSpecificDefaultWebClientSetPermission(method);
#endif
}

}  // namespace

DefaultWebClientSetPermission GetDefaultBrowserSetPermission() {
  return GetDefaultWebClientSetPermission(
      internal::WebClientSetMethod::kDefaultBrowser);
}

DefaultWebClientSetPermission GetDefaultSchemeClientSetPermission() {
  return GetDefaultWebClientSetPermission(
      internal::WebClientSetMethod::kDefaultSchemeHandler);
}

bool CanSetAsDefaultBrowser() {
  return GetDefaultBrowserSetPermission() != SET_DEFAULT_NOT_ALLOWED;
}

base::CommandLine CommandLineArgsForLauncher(
    const GURL& url,
    const std::string& extension_app_id,
    const base::FilePath& profile_path,
    const std::string& run_on_os_login_mode) {
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

  if (!run_on_os_login_mode.empty()) {
    new_cmd_line.AppendSwitchASCII(switches::kAppRunOnOsLoginMode,
                                   run_on_os_login_mode);
  }

  return new_cmd_line;
}

base::CommandLine CommandLineArgsForUrlShortcut(
    const base::FilePath& chrome_exe_program,
    const base::FilePath& profile_path,
    const GURL& url) {
  CHECK(!chrome_exe_program.empty());
  CHECK(!profile_path.empty());
  CHECK(url.is_valid());
  base::CommandLine new_cmd_line(chrome_exe_program);
  AppendProfileArgs(profile_path, &new_cmd_line);
  new_cmd_line.AppendSwitch(switches::kIgnoreProfileDirectoryIfNotExists);
  new_cmd_line.AppendArg(url.spec());

  return new_cmd_line;
}

void AppendProfileArgs(const base::FilePath& profile_path,
                       base::CommandLine* command_line) {
  DCHECK(command_line);
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();

  // Use the same UserDataDir for new launches that we currently have set.
  base::FilePath user_data_dir =
      cmd_line.GetSwitchValuePath(switches::kUserDataDir);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  policy::path_parser::CheckUserDataDirPolicy(&user_data_dir);
#endif
  if (!user_data_dir.empty()) {
    // Make sure user_data_dir is an absolute path.
    user_data_dir = base::MakeAbsoluteFilePath(user_data_dir);
    if (!user_data_dir.empty() && base::PathExists(user_data_dir))
      command_line->AppendSwitchPath(switches::kUserDataDir, user_data_dir);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::FilePath profile =
      cmd_line.GetSwitchValuePath(ash::switches::kLoginProfile);
  if (!profile.empty())
    command_line->AppendSwitchPath(ash::switches::kLoginProfile, profile);
#else
  if (!profile_path.empty())
    command_line->AppendSwitchPath(switches::kProfileDirectory,
                                   profile_path.BaseName());
#endif
}

#if !BUILDFLAG(IS_WIN)
std::u16string GetAppShortcutsSubdirName() {
  if (chrome::GetChannel() == version_info::Channel::CANARY)
    return l10n_util::GetStringUTF16(IDS_APP_SHORTCUTS_SUBDIR_NAME_CANARY);
  return l10n_util::GetStringUTF16(IDS_APP_SHORTCUTS_SUBDIR_NAME);
}
#endif  // !BUILDFLAG(IS_WIN)

///////////////////////////////////////////////////////////////////////////////
// DefaultWebClientWorker
//

void DefaultWebClientWorker::StartCheckIsDefault(
    DefaultWebClientWorkerCallback callback) {
  g_sequenced_task_runner.Get()->PostTask(
      FROM_HERE, base::BindOnce(&DefaultWebClientWorker::CheckIsDefault, this,
                                false, std::move(callback)));
}

void DefaultWebClientWorker::StartSetAsDefault(
    DefaultWebClientWorkerCallback callback) {
  g_sequenced_task_runner.Get()->PostTask(
      FROM_HERE, base::BindOnce(&DefaultWebClientWorker::SetAsDefault, this,
                                std::move(callback)));
}

///////////////////////////////////////////////////////////////////////////////
// DefaultWebClientWorker, protected:

DefaultWebClientWorker::DefaultWebClientWorker(const char* worker_name)
    : worker_name_(worker_name) {}

DefaultWebClientWorker::~DefaultWebClientWorker() = default;

void DefaultWebClientWorker::OnCheckIsDefaultComplete(
    DefaultWebClientState state,
    bool is_following_set_as_default,
    DefaultWebClientWorkerCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RunCallback(std::move(callback), state);

  if (is_following_set_as_default)
    ReportSetDefaultResult(state);
}

///////////////////////////////////////////////////////////////////////////////
// DefaultWebClientWorker, private:

void DefaultWebClientWorker::CheckIsDefault(
    bool is_following_set_as_default,
    DefaultWebClientWorkerCallback callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  DefaultWebClientState state = CheckIsDefaultImpl();
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DefaultBrowserWorker::OnCheckIsDefaultComplete, this,
                     state, is_following_set_as_default, std::move(callback)));
}

void DefaultWebClientWorker::SetAsDefault(
    DefaultWebClientWorkerCallback callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // SetAsDefaultImpl will make sure the callback is executed exactly once.
  SetAsDefaultImpl(base::BindOnce(&DefaultWebClientWorker::CheckIsDefault, this,
                                  true, std::move(callback)));
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

///////////////////////////////////////////////////////////////////////////////
// DefaultBrowserWorker
//

DefaultBrowserWorker::DefaultBrowserWorker()
    : DefaultWebClientWorker("DefaultBrowser") {}

///////////////////////////////////////////////////////////////////////////////
// DefaultBrowserWorker, private:

DefaultBrowserWorker::~DefaultBrowserWorker() = default;

// static
bool DefaultBrowserWorker::g_disable_set_as_default_for_testing = false;

// static
void DefaultBrowserWorker::DisableSetAsDefaultForTesting() {
  g_disable_set_as_default_for_testing = true;
}

DefaultWebClientState DefaultBrowserWorker::CheckIsDefaultImpl() {
  return GetDefaultBrowser();
}

void DefaultBrowserWorker::SetAsDefaultImpl(
    base::OnceClosure on_finished_callback) {
  if (!g_disable_set_as_default_for_testing) {
    switch (GetDefaultBrowserSetPermission()) {
      case SET_DEFAULT_NOT_ALLOWED:
        // This is a no-op on channels where set-default is not allowed, but not
        // an error.
        break;
      case SET_DEFAULT_UNATTENDED:
        SetAsDefaultBrowser();
        break;
      case SET_DEFAULT_INTERACTIVE:
#if BUILDFLAG(IS_WIN)
        if (interactive_permitted_) {
          win::SetAsDefaultBrowserUsingSystemSettings(
              std::move(on_finished_callback));
          // Early return because the function above takes care of calling
          // `on_finished_callback`.
          return;
        }
#endif  // BUILDFLAG(IS_WIN)
        break;
    }
  }
  std::move(on_finished_callback).Run();
}

///////////////////////////////////////////////////////////////////////////////
// DefaultSchemeClientWorker
//

DefaultSchemeClientWorker::DefaultSchemeClientWorker(const std::string& scheme)
    : DefaultWebClientWorker("DefaultSchemeClient"), scheme_(scheme) {}

DefaultSchemeClientWorker::DefaultSchemeClientWorker(const GURL& url)
    : DefaultWebClientWorker("DefaultSchemeClient"),
      scheme_(url.scheme()),
      url_(url) {}

void DefaultSchemeClientWorker::StartCheckIsDefaultAndGetDefaultClientName(
    DefaultSchemeHandlerWorkerCallback callback) {
  g_sequenced_task_runner.Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DefaultSchemeClientWorker::CheckIsDefaultAndGetDefaultClientName,
          this, std::move(callback)));
}

///////////////////////////////////////////////////////////////////////////////
// DefaultSchemeClientWorker, protected:

DefaultSchemeClientWorker::~DefaultSchemeClientWorker() = default;

void DefaultSchemeClientWorker::OnCheckIsDefaultAndGetDefaultClientNameComplete(
    DefaultWebClientState state,
    std::u16string program_name,
    DefaultSchemeHandlerWorkerCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!callback.is_null() && IsValidDefaultWebClientState(state)) {
    std::move(callback).Run(state, program_name);
  }
}

///////////////////////////////////////////////////////////////////////////////
// DefaultSchemeClientWorker, private:

void DefaultSchemeClientWorker::CheckIsDefaultAndGetDefaultClientName(
    DefaultSchemeHandlerWorkerCallback callback) {
  DCHECK(!url_.is_empty());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  DefaultWebClientState state = CheckIsDefaultImpl();
  std::u16string program_name = GetDefaultClientNameImpl();
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DefaultSchemeClientWorker::
                         OnCheckIsDefaultAndGetDefaultClientNameComplete,
                     this, state, program_name, std::move(callback)));
}

DefaultWebClientState DefaultSchemeClientWorker::CheckIsDefaultImpl() {
  return IsDefaultClientForScheme(scheme_);
}

std::u16string DefaultSchemeClientWorker::GetDefaultClientNameImpl() {
  return GetApplicationNameForScheme(url_);
}

void DefaultSchemeClientWorker::SetAsDefaultImpl(
    base::OnceClosure on_finished_callback) {
  switch (GetDefaultSchemeClientSetPermission()) {
    case SET_DEFAULT_NOT_ALLOWED:
      // Not allowed, do nothing.
      break;
    case SET_DEFAULT_UNATTENDED:
      SetAsDefaultClientForScheme(scheme_);
      break;
    case SET_DEFAULT_INTERACTIVE:
#if BUILDFLAG(IS_WIN)
      if (interactive_permitted_) {
        win::SetAsDefaultClientForSchemeUsingSystemSettings(
            scheme_, std::move(on_finished_callback));
        // Early return because the function above takes care of calling
        // `on_finished_callback`.
        return;
      }
#endif  // BUILDFLAG(IS_WIN)
      break;
  }
  std::move(on_finished_callback).Run();
}

}  // namespace shell_integration
