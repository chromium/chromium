// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/upgrade_util_win.h"

#include <objbase.h>

#include <windows.h>

#include <psapi.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <ios>
#include <string>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_process_singleton.h"
#include "chrome/browser/first_run/upgrade_util.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/win/browser_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/app_command.h"
#include "chrome/installer/util/per_install_values.h"
#include "chrome/installer/util/util_constants.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#endif

namespace {

bool GetNewerChromeFile(base::FilePath* path) {
  if (!base::PathService::Get(base::DIR_EXE, path))
    return false;
  *path = path->Append(installer::kChromeNewExe);
  return true;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Holds the result of the IPC to CoCreate `GoogleUpdate3Web`.
struct CreateGoogleUpdate3WebResult
    : public base::RefCountedThreadSafe<CreateGoogleUpdate3WebResult> {
  Microsoft::WRL::ComPtr<IStream> stream;
  base::WaitableEvent completion_event;

 private:
  friend class base::RefCountedThreadSafe<CreateGoogleUpdate3WebResult>;
  ~CreateGoogleUpdate3WebResult() = default;
};

// CoCreates the `GoogleUpdate3Web` class, and if successful, marshals the
// resulting interface into `result->stream`. Signals `result->completion_event`
// on successful or failed completion.
void CreateAndMarshalGoogleUpdate3Web(
    scoped_refptr<CreateGoogleUpdate3WebResult> result) {
  const absl::Cleanup signal_completion_event = [&result] {
    result->completion_event.Signal();
  };

  Microsoft::WRL::ComPtr<IUnknown> unknown;
  {
    TRACE_EVENT0("startup", "InvokeGoogleUpdateForRename CoCreateInstance");
    const HRESULT hr =
        ::CoCreateInstance(__uuidof(GoogleUpdate3WebSystemClass), nullptr,
                           CLSCTX_ALL, IID_PPV_ARGS(&unknown));
    if (FAILED(hr)) {
      TRACE_EVENT_INSTANT1(
          "startup", "InvokeGoogleUpdateForRename CoCreateInstance failed",
          TRACE_EVENT_SCOPE_THREAD, "hr", hr);
      LOG(ERROR) << "CoCreate GoogleUpdate3WebSystemClass failed; hr = "
                 << std::hex << hr;
      return;
    }
  }
  const HRESULT hr = ::CoMarshalInterThreadInterfaceInStream(
      __uuidof(IUnknown), unknown.Get(), &result->stream);
  if (FAILED(hr)) {
    TRACE_EVENT_INSTANT1("startup",
                         "InvokeGoogleUpdateForRename "
                         "CoMarshalInterThreadInterfaceInStream failed",
                         TRACE_EVENT_SCOPE_THREAD, "hr", hr);
    LOG(ERROR) << "CoMarshalInterThreadInterfaceInStream "
                  "GoogleUpdate3WebSystemClass failed; hr = "
               << std::hex << hr;
  }
}

// CoCreates the Google Update `GoogleUpdate3WebSystemClass` in a `ThreadPool`
// thread with a timeout, if the `ThreadPool` is operational. The starting value
// for the timeout is 15 seconds. If the CoCreate times out, the timeout is
// increased by 15 seconds at each failed attempt and persisted for the next
// attempt.
//
// If the `ThreadPool` is not operational, the CoCreate is done
// without a timeout.
Microsoft::WRL::ComPtr<IUnknown> CreateGoogleUpdate3Web() {
  constexpr int kDefaultTimeoutIncrementSeconds = 15;
  constexpr base::TimeDelta kMaxTimeAfterSystemStartup = base::Seconds(150);

  auto result = base::MakeRefCounted<CreateGoogleUpdate3WebResult>();
  if (base::ThreadPool::CreateCOMSTATaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING})
          ->PostTask(
              FROM_HERE,
              base::BindOnce(&CreateAndMarshalGoogleUpdate3Web, result))) {
    installer::PerInstallValue creation_timeout(
        L"ProcessLauncherCreationTimeout");
    const base::TimeDelta timeout = base::Seconds(
        creation_timeout.Get()
            .value_or(base::Value(kDefaultTimeoutIncrementSeconds))
            .GetIfInt()
            .value_or(kDefaultTimeoutIncrementSeconds));
    const base::ElapsedTimer timer;
    const bool is_at_startup =
        base::SysInfo::Uptime() <= kMaxTimeAfterSystemStartup;
    if (!result->completion_event.TimedWait(timeout)) {
      base::UmaHistogramMediumTimes(
          is_at_startup
              ? "Startup.CreateProcessLauncher2.TimedWaitFailedAtStartup"
              : "Startup.CreateProcessLauncher2.TimedWaitFailed",
          timer.Elapsed());
      creation_timeout.Set(base::Value(static_cast<int>(timeout.InSeconds()) +
                                       kDefaultTimeoutIncrementSeconds));
      TRACE_EVENT_INSTANT0(
          "startup", "InvokeGoogleUpdateForRename CoCreateInstance timed out",
          TRACE_EVENT_SCOPE_THREAD);
      LOG(ERROR) << "CoCreate GoogleUpdate3WebSystemClass timed out";
      return {};
    }

    if (!result->stream) {
      return {};
    }
    base::UmaHistogramMediumTimes(
        is_at_startup
            ? "Startup.CreateProcessLauncher2.TimedWaitSucceededAtStartup"
            : "Startup.CreateProcessLauncher2.TimedWaitSucceeded",
        timer.Elapsed());

    Microsoft::WRL::ComPtr<IUnknown> unknown;
    const HRESULT hr =
        ::CoUnmarshalInterface(result->stream.Get(), __uuidof(IUnknown),
                               IID_PPV_ARGS_Helper(&unknown));
    if (FAILED(hr)) {
      TRACE_EVENT_INSTANT1(
          "startup", "InvokeGoogleUpdateForRename CoUnmarshalInterface failed",
          TRACE_EVENT_SCOPE_THREAD, "hr", hr);
      LOG(ERROR)
          << "CoUnmarshalInterface GoogleUpdate3WebSystemClass failed; hr = "
          << std::hex << hr;
      return {};
    }

    return unknown;
  }

  // The task could not be posted to the task runner, so CoCreate without a
  // timeout. This could happen in shutdown, where the `ThreadPool` is not
  // operational.
  {
    TRACE_EVENT0("startup", "InvokeGoogleUpdateForRename CoCreateInstance");
    Microsoft::WRL::ComPtr<IUnknown> unknown;
    const HRESULT hr =
        ::CoCreateInstance(__uuidof(GoogleUpdate3WebSystemClass), nullptr,
                           CLSCTX_ALL, IID_PPV_ARGS(&unknown));
    if (FAILED(hr)) {
      TRACE_EVENT_INSTANT1(
          "startup", "InvokeGoogleUpdateForRename CoCreateInstance failed",
          TRACE_EVENT_SCOPE_THREAD, "hr", hr);
      LOG(ERROR) << "CoCreate GoogleUpdate3WebSystemClass failed; hr = "
                 << std::hex << hr;
      return {};
    }

    return unknown;
  }
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

bool InvokeGoogleUpdateForRename() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // This has been identified as very slow on some startups. Detailed trace
  // events below try to shine a light on each steps. crbug.com/1252004
  TRACE_EVENT0("startup", "upgrade_util::InvokeGoogleUpdateForRename");

  Microsoft::WRL::ComPtr<IUnknown> unknown = CreateGoogleUpdate3Web();
  if (!unknown) {
    return false;
  }

  // Chrome queries for the SxS IIDs first, with a fallback to the legacy IID,
  // to make sure that marshaling loads the proxy/stub from the correct (HKLM)
  // hive.
  Microsoft::WRL::ComPtr<IGoogleUpdate3Web> update3web;
  if (HRESULT hr = unknown.CopyTo(__uuidof(IGoogleUpdate3WebSystem),
                                  IID_PPV_ARGS_Helper(&update3web));
      FAILED(hr)) {
    hr = unknown.As(&update3web);
    if (FAILED(hr)) {
      TRACE_EVENT_INSTANT1(
          "startup", "InvokeGoogleUpdateForRename QI IGoogleUpdate3Web failed",
          TRACE_EVENT_SCOPE_THREAD, "hr", hr);
      LOG(ERROR) << "QI IGoogleUpdate3Web failed; hr = " << std::hex << hr;
      return false;
    }
  }

  Microsoft::WRL::ComPtr<IAppBundleWeb> bundle;
  {
    Microsoft::WRL::ComPtr<IDispatch> dispatch;
    if (HRESULT hr = update3web->createAppBundleWeb(&dispatch); FAILED(hr)) {
      TRACE_EVENT_INSTANT1(
          "startup", "InvokeGoogleUpdateForRename createAppBundleWeb failed",
          TRACE_EVENT_SCOPE_THREAD, "hr", hr);
      LOG(ERROR) << "createAppBundleWeb failed; hr = " << std::hex << hr;
      return false;
    }

    if (HRESULT hr = dispatch.CopyTo(__uuidof(IAppBundleWebSystem),
                                     IID_PPV_ARGS_Helper(&bundle));
        FAILED(hr)) {
      hr = dispatch.As(&bundle);
      if (FAILED(hr)) {
        TRACE_EVENT_INSTANT1(
            "startup", "InvokeGoogleUpdateForRename QI IAppBundleWeb failed",
            TRACE_EVENT_SCOPE_THREAD, "hr", hr);
        LOG(ERROR) << "QI IAppBundleWeb failed; hr = " << std::hex << hr;
        return false;
      }
    }
  }

  if (HRESULT hr = bundle->initialize(); FAILED(hr)) {
    TRACE_EVENT_INSTANT1(
        "startup", "InvokeGoogleUpdateForRename bundle->initialize failed",
        TRACE_EVENT_SCOPE_THREAD, "hr", hr);
    LOG(ERROR) << "bundle->initialize failed; hr = " << std::hex << hr;
    return false;
  }

  if (HRESULT hr = bundle->createInstalledApp(
          base::win::ScopedBstr(install_static::GetAppGuid()).Get());
      FAILED(hr)) {
    TRACE_EVENT_INSTANT1(
        "startup",
        "InvokeGoogleUpdateForRename bundle->createInstalledApp failed",
        TRACE_EVENT_SCOPE_THREAD, "hr", hr);
    LOG(ERROR) << "bundle->createInstalledApp failed; hr = " << std::hex << hr;
    return false;
  }

  Microsoft::WRL::ComPtr<IAppWeb> app;
  {
    Microsoft::WRL::ComPtr<IDispatch> app_dispatch;
    if (HRESULT hr = bundle->get_appWeb(0, &app_dispatch); FAILED(hr)) {
      TRACE_EVENT_INSTANT1(
          "startup", "InvokeGoogleUpdateForRename bundle->get_appWeb failed",
          TRACE_EVENT_SCOPE_THREAD, "hr", hr);
      LOG(ERROR) << "bundle->get_appWeb failed; hr = " << std::hex << hr;
      return false;
    }

    if (HRESULT hr = app_dispatch.CopyTo(__uuidof(IAppWebSystem),
                                         IID_PPV_ARGS_Helper(&app));
        FAILED(hr)) {
      hr = app_dispatch.As(&app);
      if (FAILED(hr)) {
        TRACE_EVENT_INSTANT1("startup",
                             "InvokeGoogleUpdateForRename QI IAppWeb failed",
                             TRACE_EVENT_SCOPE_THREAD, "hr", hr);
        LOG(ERROR) << "QI IAppWeb failed; hr = " << std::hex << hr;
        return false;
      }
    }
  }

  Microsoft::WRL::ComPtr<IAppCommandWeb> app_command_web;
  {
    Microsoft::WRL::ComPtr<IDispatch> command_dispatch;
    if (HRESULT hr = app->get_command(
            base::win::ScopedBstr(installer::kCmdRenameChromeExe).Get(),
            &command_dispatch);
        FAILED(hr)) {
      TRACE_EVENT_INSTANT1(
          "startup", "InvokeGoogleUpdateForRename app->get_command failed",
          TRACE_EVENT_SCOPE_THREAD, "hr", hr);
      LOG(ERROR) << "app->get_command failed; hr = " << std::hex << hr;
      return false;
    }

    if (HRESULT hr =
            command_dispatch.CopyTo(__uuidof(IAppCommandWebSystem),
                                    IID_PPV_ARGS_Helper(&app_command_web));
        FAILED(hr)) {
      hr = command_dispatch.As(&app_command_web);
      if (FAILED(hr)) {
        TRACE_EVENT_INSTANT1(
            "startup", "InvokeGoogleUpdateForRename QI IAppCommandWeb failed",
            TRACE_EVENT_SCOPE_THREAD, "hr", hr);
        LOG(ERROR) << "QI IAppCommandWeb failed; hr = " << std::hex << hr;
        return false;
      }
    }
  }

  {
    TRACE_EVENT0("startup", "InvokeGoogleUpdateForRename execute");
    if (HRESULT hr =
            app_command_web->execute(base::win::ScopedVariant::kEmptyVariant,
                                     base::win::ScopedVariant::kEmptyVariant,
                                     base::win::ScopedVariant::kEmptyVariant,
                                     base::win::ScopedVariant::kEmptyVariant,
                                     base::win::ScopedVariant::kEmptyVariant,
                                     base::win::ScopedVariant::kEmptyVariant,
                                     base::win::ScopedVariant::kEmptyVariant,
                                     base::win::ScopedVariant::kEmptyVariant,
                                     base::win::ScopedVariant::kEmptyVariant);
        FAILED(hr)) {
      TRACE_EVENT_INSTANT1(
          "startup",
          "InvokeGoogleUpdateForRename app_command_web->execute failed",
          TRACE_EVENT_SCOPE_THREAD, "hr", hr);
      LOG(ERROR) << "app_command_web->execute failed; hr = " << std::hex << hr;
      return false;
    }

    UINT status = 0;
    for (const auto deadline = base::TimeTicks::Now() + base::Seconds(60);
         base::TimeTicks::Now() < deadline;
         base::PlatformThread::Sleep(base::Seconds(1))) {
      if (HRESULT hr = app_command_web->get_status(&status); FAILED(hr)) {
        TRACE_EVENT_INSTANT1(
            "startup",
            "InvokeGoogleUpdateForRename app_command_web->get_status failed",
            TRACE_EVENT_SCOPE_THREAD, "hr", hr);
        LOG(ERROR) << "app_command_web->get_status failed; hr = " << std::hex
                   << hr;
        return false;
      }
      if (status == COMMAND_STATUS_COMPLETE) {
        break;
      }
    }
    if (status != COMMAND_STATUS_COMPLETE) {
      TRACE_EVENT_INSTANT1(
          "startup", "InvokeGoogleUpdateForRename !COMMAND_STATUS_COMPLETE",
          TRACE_EVENT_SCOPE_THREAD, "status", status);
      LOG(ERROR) << "AppCommand timed out with status code " << status;
      return false;
    }
  }

  DWORD exit_code = 0;
  if (HRESULT hr = app_command_web->get_exitCode(&exit_code); FAILED(hr)) {
    TRACE_EVENT_INSTANT1(
        "startup",
        "InvokeGoogleUpdateForRename app_command_web->get_exitCode failed",
        TRACE_EVENT_SCOPE_THREAD, "hr", hr);
    LOG(ERROR) << "app_command_web->get_exitCode failed; hr = " << std::hex
               << hr;
    return false;
  }

  if (exit_code != installer::RENAME_SUCCESSFUL) {
    TRACE_EVENT_INSTANT1("startup",
                         "InvokeGoogleUpdateForRename !RENAME_SUCCESSFUL",
                         TRACE_EVENT_SCOPE_THREAD, "exit_code", exit_code);
    LOG(ERROR) << "Rename process failed with exit code " << exit_code;
    return false;
  }

  TRACE_EVENT_INSTANT0("startup",
                       "InvokeGoogleUpdateForRename RENAME_SUCCESSFUL",
                       TRACE_EVENT_SCOPE_THREAD);

  return true;
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

}  // namespace

namespace upgrade_util {

bool RelaunchChromeBrowserImpl(const base::CommandLine& command_line) {
  TRACE_EVENT0("startup", "upgrade_util::RelaunchChromeBrowserImpl");

  base::FilePath chrome_exe;
  if (!base::PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
  }

  // Explicitly make sure to relaunch chrome.exe rather than old_chrome.exe.
  // This can happen when old_chrome.exe is launched by a user.
  base::CommandLine chrome_exe_command_line = command_line;
  chrome_exe_command_line.SetProgram(
      chrome_exe.DirName().Append(installer::kChromeExe));

  // Set the working directory to the exe's directory. This avoids a handle to
  // the version directory being kept open in the relaunched child process.
  base::LaunchOptions launch_options;
  launch_options.current_directory = chrome_exe.DirName();
  // Give the new process the right to bring its windows to the foreground.
  launch_options.grant_foreground_privilege = true;
  return base::LaunchProcess(chrome_exe_command_line, launch_options).IsValid();
}

bool IsUpdatePendingRestart() {
  TRACE_EVENT0("startup", "upgrade_util::IsUpdatePendingRestart");
  base::FilePath new_chrome_exe;
  if (!GetNewerChromeFile(&new_chrome_exe))
    return false;
  return base::PathExists(new_chrome_exe);
}

bool SwapNewChromeExeIfPresent() {
  if (!IsUpdatePendingRestart())
    return false;

  TRACE_EVENT0("startup", "upgrade_util::SwapNewChromeExeIfPresent");

  // Renaming the chrome executable requires the process singleton to avoid
  // any race condition.
  CHECK(ChromeProcessSingleton::IsSingletonInstance());

  // If this is a system-level install, ask Google Update to launch an elevated
  // process to rename Chrome executables.
  if (install_static::IsSystemInstall())
    return InvokeGoogleUpdateForRename();

  // If this is a user-level install, directly launch a process to rename Chrome
  // executables. Obtain the command to launch the process from the registry.
  installer::AppCommand rename_cmd(installer::kCmdRenameChromeExe, {});
  if (!rename_cmd.Initialize(HKEY_CURRENT_USER))
    return false;

  base::LaunchOptions options;
  options.wait = true;
  options.start_hidden = true;
  ::SetLastError(ERROR_SUCCESS);
  base::Process process =
      base::LaunchProcess(rename_cmd.command_line(), options);
  if (!process.IsValid()) {
    PLOG(ERROR) << "Launch rename process failed";
    return false;
  }

  DWORD exit_code;
  if (!::GetExitCodeProcess(process.Handle(), &exit_code)) {
    PLOG(ERROR) << "GetExitCodeProcess of rename process failed";
    return false;
  }

  if (exit_code != installer::RENAME_SUCCESSFUL) {
    LOG(ERROR) << "Rename process failed with exit code " << exit_code;
    return false;
  }

  return true;
}

bool IsRunningOldChrome() {
  TRACE_EVENT0("startup", "upgrade_util::IsRunningOldChrome");
  // This figures out the actual file name that the section containing the
  // mapped exe refers to. This is used instead of GetModuleFileName because the
  // .exe may have been renamed out from under us while we've been running which
  // GetModuleFileName won't notice.
  wchar_t mapped_file_name[MAX_PATH * 2] = {};

  if (!::GetMappedFileName(::GetCurrentProcess(),
                           reinterpret_cast<void*>(::GetModuleHandle(NULL)),
                           mapped_file_name, std::size(mapped_file_name))) {
    return false;
  }

  base::FilePath file_name(base::FilePath(mapped_file_name).BaseName());
  return base::FilePath::CompareEqualIgnoreCase(file_name.value(),
                                                installer::kChromeOldExe);
}

bool DoUpgradeTasks(const base::CommandLine& command_line) {
  TRACE_EVENT0("startup", "upgrade_util::DoUpgradeTasks");
  // If there is no other instance already running then check if there is a
  // pending update and complete it by performing the swap and then relaunch.

  // Upgrade tasks require the process singleton to avoid any race condition.
  CHECK(ChromeProcessSingleton::IsSingletonInstance());

  bool did_swap = false;
  if (!browser_util::IsBrowserAlreadyRunning())
    did_swap = SwapNewChromeExeIfPresent();

  // We don't need to relaunch if we didn't swap and we aren't running stale
  // binaries.
  if (!did_swap && !IsRunningOldChrome()) {
    return false;
  }

  // At this point the chrome.exe has been swapped with the new one.
  if (!RelaunchChromeBrowser(command_line)) {
    // The relaunch failed. Feel free to panic now.
    DUMP_WILL_BE_NOTREACHED();
  }
  return true;
}

}  // namespace upgrade_util
