// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_crash_reporter_client.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/crash/core/common/crash_keys.h"
#include "content/public/common/content_switches.h"
#include "services/service_manager/embedder/switches.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "components/upload_list/crash_upload_list.h"
#include "components/version_info/version_info_values.h"
#endif

#if defined(OS_POSIX)
#include "base/debug/dump_without_crashing.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/common/chrome_descriptors.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/common/channel_info.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/version_info/version_info.h"
#endif

void ChromeCrashReporterClient::Create() {
  static base::NoDestructor<ChromeCrashReporterClient> crash_client;
  crash_reporter::SetCrashReporterClient(crash_client.get());

  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write crash dumps can be set.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string alternate_crash_dump_location;
  base::FilePath crash_dumps_dir_path;
  if (env->GetVar("BREAKPAD_DUMP_LOCATION", &alternate_crash_dump_location)) {
    crash_dumps_dir_path =
        base::FilePath::FromUTF8Unsafe(alternate_crash_dump_location);
  } else if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                 "breakpad-dump-location")) {
    // This is needed for Android tests, where we want dumps to go to a location
    // where they don't get uploaded/deleted, but we can't use environment
    // variables.
    crash_dumps_dir_path =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            "breakpad-dump-location");
  }
  if (!crash_dumps_dir_path.empty()) {
    base::PathService::Override(chrome::DIR_CRASH_DUMPS, crash_dumps_dir_path);
  }
}

#if defined(OS_CHROMEOS)
// static
bool ChromeCrashReporterClient::ShouldPassCrashLoopBefore(
    const std::string& process_type) {
  if (process_type == ::switches::kRendererProcess ||
      process_type == ::switches::kUtilityProcess ||
      process_type == ::switches::kPpapiPluginProcess ||
      process_type == service_manager::switches::kZygoteProcess) {
    // These process types never cause a log-out, even if they crash. So the
    // normal crash handling process should work fine; we shouldn't need to
    // invoke the special crash-loop mode.
    return false;
  }
  return true;
}
#endif

ChromeCrashReporterClient::ChromeCrashReporterClient() {}

ChromeCrashReporterClient::~ChromeCrashReporterClient() {}

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
void ChromeCrashReporterClient::SetCrashReporterClientIdFromGUID(
    const std::string& client_guid) {
  crash_keys::SetMetricsClientIdFromGUID(client_guid);
}
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
void ChromeCrashReporterClient::GetProductNameAndVersion(
    const char** product_name,
    const char** version) {
  DCHECK(product_name);
  DCHECK(version);
#if defined(OS_ANDROID)
  *product_name = "Chrome_Android";
#elif defined(OS_CHROMEOS)
  *product_name = "Chrome_ChromeOS";
#else  // OS_LINUX
#if !defined(ADDRESS_SANITIZER)
  *product_name = "Chrome_Linux";
#else
  *product_name = "Chrome_Linux_ASan";
#endif
#endif

  *version = PRODUCT_VERSION;
}

void ChromeCrashReporterClient::GetProductNameAndVersion(
    std::string* product_name,
    std::string* version,
    std::string* channel) {
  const char* c_product_name;
  const char* c_version;
  GetProductNameAndVersion(&c_product_name, &c_version);
  *product_name = c_product_name;
  *version = c_version;
  *channel = chrome::GetChannelName();
}

base::FilePath ChromeCrashReporterClient::GetReporterLogFilename() {
  return base::FilePath(CrashUploadList::kReporterLogFilename);
}
#endif

bool ChromeCrashReporterClient::GetCrashDumpLocation(
    base::FilePath* crash_dir) {
  return base::PathService::Get(chrome::DIR_CRASH_DUMPS, crash_dir);
}

#if defined(OS_MACOSX) || defined(OS_LINUX)
bool ChromeCrashReporterClient::GetCrashMetricsLocation(
    base::FilePath* metrics_dir) {
  return base::PathService::Get(chrome::DIR_USER_DATA, metrics_dir);
}
#endif  // OS_MACOSX || OS_LINUX

bool ChromeCrashReporterClient::IsRunningUnattended() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  return env->HasVar(env_vars::kHeadless);
}

bool ChromeCrashReporterClient::GetCollectStatsConsent() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  bool is_official_chrome_build = true;
#else
  bool is_official_chrome_build = false;
#endif

#if defined(OS_CHROMEOS)
  bool is_guest_session = base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kGuestSession);
  bool is_stable_channel =
      chrome::GetChannel() == version_info::Channel::STABLE;

  if (is_guest_session && is_stable_channel) {
    VLOG(1) << "GetCollectStatsConsent(): is_guest_session " << is_guest_session
            << " && is_stable_channel " << is_stable_channel
            << " so returning false";
    return false;
  }
#endif  // defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
  // TODO(jcivelli): we should not initialize the crash-reporter when it was not
  // enabled. Right now if it is disabled we still generate the minidumps but we
  // do not upload them.
  return is_official_chrome_build;
#else  // !defined(OS_ANDROID)
  if (!is_official_chrome_build) {
    VLOG(1) << "GetCollectStatsConsent(): is_official_chrome_build is false "
            << "so returning false";
    return false;
  }
  bool settings_consent = GoogleUpdateSettings::GetCollectStatsConsent();
  VLOG(1) << "GetCollectStatsConsent(): settings_consent: " << settings_consent
          << " so returning that";
  return settings_consent;
#endif  // defined(OS_ANDROID)
}

#if defined(OS_ANDROID)
int ChromeCrashReporterClient::GetAndroidMinidumpDescriptor() {
  return kAndroidMinidumpDescriptor;
}
#endif

#if defined(OS_LINUX)
bool ChromeCrashReporterClient::ShouldMonitorCrashHandlerExpensively() {
  // TODO(jperaza): Turn this on less frequently for stable channels when
  // Crashpad is always enabled on Linux. Consider combining with the
  // macOS implementation.
  return true;
}
#endif  // OS_LINUX

bool ChromeCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return process_type == switches::kRendererProcess ||
         process_type == switches::kPpapiPluginProcess ||
         process_type == service_manager::switches::kZygoteProcess ||
         process_type == switches::kGpuProcess ||
         process_type == switches::kUtilityProcess;
}
