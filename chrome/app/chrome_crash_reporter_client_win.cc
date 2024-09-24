// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(ananta/scottmg)
// Add test coverage for Crashpad.
#include "chrome/app/chrome_crash_reporter_client_win.h"

#include <windows.h>

#include <assert.h>
#include <shellapi.h>

#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/current_module.h"
#include "chrome/chrome_elf/chrome_elf_constants.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/user_data_dir.h"
#include "components/crash/core/app/crashpad.h"
#include "components/version_info/channel.h"

ChromeCrashReporterClient::ChromeCrashReporterClient() {}

ChromeCrashReporterClient::~ChromeCrashReporterClient() {}

// static
void ChromeCrashReporterClient::InitializeCrashReportingForProcess() {
  static ChromeCrashReporterClient* instance = nullptr;
  if (instance)
    return;

  instance = new ChromeCrashReporterClient();
  ANNOTATE_LEAKING_OBJECT_PTR(instance);

  std::wstring process_type = install_static::GetCommandLineSwitchValue(
      ::GetCommandLine(), install_static::kProcessType);

  // Don't set up Crashpad crash reporting in the Crashpad handler itself, nor
  // in the fallback crash handler for the Crashpad handler process.
  if (process_type != install_static::kCrashpadHandler &&
      process_type != install_static::kFallbackHandler) {
    crash_reporter::SetCrashReporterClient(instance);

    std::wstring user_data_dir;
    if (process_type.empty())
      install_static::GetUserDataDirectory(&user_data_dir, nullptr);

    // TODO(wfh): Add a DCHECK for success. See https://crbug.com/1329269.
    std::ignore = crash_reporter::InitializeCrashpadWithEmbeddedHandler(
        /*initial_client=*/process_type.empty(),
        install_static::WideToUTF8(process_type),
        install_static::WideToUTF8(user_data_dir), base::FilePath());
  }
}

bool ChromeCrashReporterClient::GetAlternativeCrashDumpLocation(
    std::wstring* crash_dir) {
  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write breakpad crash dumps can be set.
  *crash_dir = install_static::GetEnvironmentString(L"BREAKPAD_DUMP_LOCATION");
  return !crash_dir->empty();
}

void ChromeCrashReporterClient::GetProductNameAndVersion(
    const std::wstring& exe_path,
    std::wstring* product_name,
    std::wstring* version,
    std::wstring* special_build,
    std::wstring* channel_name) {
  assert(product_name);
  assert(version);
  assert(special_build);
  assert(channel_name);

  install_static::GetExecutableVersionDetails(exe_path, product_name, version,
                                              special_build, channel_name);
}

bool ChromeCrashReporterClient::ShouldShowRestartDialog(std::wstring* title,
                                                        std::wstring* message,
                                                        bool* is_rtl_locale) {
  if (!install_static::HasEnvironmentVariable(install_static::kShowRestart) ||
      !install_static::HasEnvironmentVariable(install_static::kRestartInfo)) {
    return false;
  }

  std::wstring restart_info =
      install_static::GetEnvironmentString(install_static::kRestartInfo);

  // The CHROME_RESTART var contains the dialog strings separated by '|'.
  // See ChromeBrowserMainPartsWin::PrepareRestartOnCrashEnviroment()
  // for details.
  std::vector<std::wstring> dlg_strings = install_static::TokenizeString(
      restart_info, L'|', true);  // true = Trim whitespace.

  if (dlg_strings.size() < 3)
    return false;

  *title = dlg_strings[0];
  *message = dlg_strings[1];
  *is_rtl_locale = dlg_strings[2] == install_static::kRtlLocale;
  return true;
}

bool ChromeCrashReporterClient::AboutToRestart() {
  if (!install_static::HasEnvironmentVariable(install_static::kRestartInfo))
    return false;

  install_static::SetEnvironmentString(install_static::kShowRestart, L"1");
  return true;
}

bool ChromeCrashReporterClient::GetIsPerUserInstall() {
  return !install_static::IsSystemInstall();
}

bool ChromeCrashReporterClient::GetShouldDumpLargerDumps() {
  // Capture larger dumps for Google Chrome beta, dev, and canary channels, and
  // Chromium builds. The Google Chrome stable channel uses smaller dumps.
  return install_static::GetChromeChannel() != version_info::Channel::STABLE;
}

int ChromeCrashReporterClient::GetResultCodeRespawnFailed() {
  return chrome::RESULT_CODE_RESPAWN_FAILED;
}

bool ChromeCrashReporterClient::ReportingIsEnforcedByPolicy(
    bool* crashpad_enabled) {
  // Determine whether configuration management allows loading the crash
  // reporter.
  // Since the configuration management infrastructure is not initialized at
  // this point, we read the corresponding registry key directly. The return
  // status indicates whether policy data was successfully read. If it is true,
  // |breakpad_enabled| contains the value set by policy.
  return install_static::ReportingIsEnforcedByPolicy(crashpad_enabled);
}

bool ChromeCrashReporterClient::GetCrashDumpLocation(std::wstring* crash_dir) {
  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write breakpad crash dumps can be set.
  // If this environment variable exists, then for the time being,
  // short-circuit how it's handled on Windows. Honoring this
  // variable is required in order to symbolize stack traces in
  // Telemetry based tests: http://crbug.com/561763.
  if (GetAlternativeCrashDumpLocation(crash_dir))
    return true;

  *crash_dir = install_static::GetCrashDumpLocation();
  return !crash_dir->empty();
}

bool ChromeCrashReporterClient::GetCrashMetricsLocation(
    std::wstring* metrics_dir) {
  if (!GetCollectStatsConsent())
    return false;
  install_static::GetUserDataDirectory(metrics_dir, nullptr);
  return !metrics_dir->empty();
}

bool ChromeCrashReporterClient::IsRunningUnattended() {
  return install_static::HasEnvironmentVariable(install_static::kHeadless);
}

bool ChromeCrashReporterClient::GetCollectStatsConsent() {
  return install_static::GetCollectStatsConsent();
}

bool ChromeCrashReporterClient::GetCollectStatsInSample() {
  return install_static::GetCollectStatsInSample();
}

bool ChromeCrashReporterClient::ShouldMonitorCrashHandlerExpensively() {
  // The expensive mechanism dedicates a process to be crashpad_handler's own
  // crashpad_handler. In Google Chrome, scale back on this in the more stable
  // channels. There's a fallback crash handler that can catch crashes when this
  // expensive mechanism isn't used, although the fallback crash handler has
  // different characteristics so it's desirable to use the expensive mechanism
  // at least some of the time.
  double probability;
  switch (install_static::GetChromeChannel()) {
    case version_info::Channel::STABLE:
      return false;

    case version_info::Channel::BETA:
      probability = 0.1;
      break;

    case version_info::Channel::DEV:
      probability = 0.25;
      break;

    default:
      probability = 0.5;
      break;
  }

  return base::RandDouble() < probability;
}

bool ChromeCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  // This is not used by Crashpad (at least on Windows).
  NOTREACHED_IN_MIGRATION();
  return true;
}

std::wstring ChromeCrashReporterClient::GetWerRuntimeExceptionModule() {
  // We require that chrome_wer.dll is installed next to chrome_elf.dll. This
  // approach means we don't need to check for the dll's existence on disk early
  // in the process lifetime - we never load this dll ourselves - it is only
  // loaded by WerFault.exe after certain crashes. We do not use base::FilePath
  // and friends as chrome_elf will eventually not depend on //base.

  wchar_t elf_file[MAX_PATH];
  DWORD len = GetModuleFileName(CURRENT_MODULE(), elf_file, MAX_PATH);
  // On error return an empty path to indicate than a module is not to be
  // registered. This is harmless.
  if (len == 0 || len == MAX_PATH)
    return std::wstring();

  wchar_t elf_dir[MAX_PATH];
  wchar_t* file_start = nullptr;
  DWORD dir_len = GetFullPathName(elf_file, MAX_PATH, elf_dir, &file_start);
  if (dir_len == 0 || dir_len > len || !file_start)
    return std::wstring();

  // file_start points to the start of the filename in the elf_dir buffer.
  return std::wstring(elf_dir, file_start).append(kWerDll);
}
