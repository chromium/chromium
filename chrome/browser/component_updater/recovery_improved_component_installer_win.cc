// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/branding_buildflags.h"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

#include <windows.h>

#include <wrl/client.h>

#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/component_updater/component_updater_utils.h"
#include "chrome/browser/component_updater/recovery_improved_component_installer.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/install_static/install_util.h"
#include "components/version_info/version_info.h"

namespace component_updater {

namespace {

const base::FilePath::CharType kRecoveryFileName[] =
    FILE_PATH_LITERAL("ChromeRecovery.exe");

// Returns the Chrome's appid registered with Google Update for updates.
std::string GetBrowserAppId() {
  return base::WideToUTF8(install_static::GetAppGuid());
}

// Returns the current browser version.
std::string GetBrowserVersion() {
  return version_info::GetVersion().GetString();
}

// Instantiates the elevator service, calls its elevator interface, then
// blocks waiting for the recovery processes to exit. Returns the result
// of the recovery as a tuple.
std::tuple<bool, int, int> RunRecoveryCRXElevated(
    const base::FilePath& crx_path,
    const std::string& browser_appid,
    const std::string& browser_version,
    const std::string& session_id) {
  Microsoft::WRL::ComPtr<IElevator> elevator;
  HRESULT hr = CoCreateInstance(
      install_static::GetElevatorClsid(), nullptr, CLSCTX_LOCAL_SERVER,
      install_static::GetElevatorIid(), IID_PPV_ARGS_Helper(&elevator));
  if (FAILED(hr)) {
    return {false, static_cast<int>(hr), 0};
  }

  hr = CoSetProxyBlanket(
      elevator.Get(), RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING);
  if (FAILED(hr)) {
    return {false, static_cast<int>(hr), 0};
  }

  ULONG_PTR proc_handle = 0;
  hr = elevator->RunRecoveryCRXElevated(
      crx_path.value().c_str(), base::UTF8ToWide(browser_appid).c_str(),
      base::UTF8ToWide(browser_version).c_str(),
      base::UTF8ToWide(session_id).c_str(), base::Process::Current().Pid(),
      &proc_handle);
  if (FAILED(hr)) {
    return {false, static_cast<int>(hr), 0};
  }

  int exit_code = 0;
  const base::TimeDelta kMaxWaitTime = base::Seconds(600);
  base::Process process(reinterpret_cast<base::ProcessHandle>(proc_handle));
  const bool succeeded =
      process.WaitForExitWithTimeout(kMaxWaitTime, &exit_code);
  return {succeeded, exit_code, 0};
}

// Handles the |run| action for the recovery component for Windows.
class RecoveryComponentActionHandlerWin
    : public RecoveryComponentActionHandler {
 public:
  RecoveryComponentActionHandlerWin() = default;
  RecoveryComponentActionHandlerWin(const RecoveryComponentActionHandlerWin&) =
      delete;
  RecoveryComponentActionHandlerWin& operator=(
      const RecoveryComponentActionHandlerWin&) = delete;

 private:
  ~RecoveryComponentActionHandlerWin() override = default;

  // Overrides for RecoveryComponentActionHandler.
  base::CommandLine MakeCommandLine(
      const base::FilePath& unpack_path) const override;
  void PrepareFiles(const base::FilePath& unpack_path) const override;
  void Elevate(Callback callback) override;

  // Calls the elevator service to handle the CRX. Since the invocation of
  // the elevator service consists of several Windows COM IPC calls, a
  // certain type of task runner is necessary to initialize a COM apartment.
  void RunElevatedInSTA(Callback callback);
};

base::CommandLine RecoveryComponentActionHandlerWin::MakeCommandLine(
    const base::FilePath& unpack_path) const {
  base::CommandLine command_line(unpack_path.Append(kRecoveryFileName));
  command_line.AppendSwitchASCII("browser-version", GetBrowserVersion());
  command_line.AppendSwitchASCII("sessionid", session_id());
  const auto app_guid = GetBrowserAppId();
  if (!app_guid.empty()) {
    command_line.AppendSwitchASCII("appguid", app_guid);
  }
  return command_line;
}

void RecoveryComponentActionHandlerWin::PrepareFiles(
    const base::FilePath& unpack_path) const {
  // Nothing to do.
}

void RecoveryComponentActionHandlerWin::Elevate(Callback callback) {
  base::ThreadPool::CreateCOMSTATaskRunner(
      kThreadPoolTaskTraitsRunCommand,
      base::SingleThreadTaskRunnerThreadMode::DEDICATED)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&RecoveryComponentActionHandlerWin::RunElevatedInSTA,
                         this, std::move(callback)));
}

void RecoveryComponentActionHandlerWin::RunElevatedInSTA(Callback callback) {
  auto [succeeded, error_code, extra_code] = RunRecoveryCRXElevated(
      crx_path(), GetBrowserAppId(), GetBrowserVersion(), session_id());
  main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), succeeded, error_code, extra_code));
}

}  // namespace

scoped_refptr<update_client::ActionHandler>
RecoveryComponentActionHandler::MakeActionHandler() {
  return base::MakeRefCounted<RecoveryComponentActionHandlerWin>();
}

}  // namespace component_updater

#endif  // GOOGLE_CHROME_BRANDING
