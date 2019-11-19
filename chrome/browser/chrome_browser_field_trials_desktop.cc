// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials_desktop.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <map>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/activity_tracker.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/common/content_switches.h"
#include "media/media_buildflags.h"

#if defined(OS_WIN)
#include "base/win/pe_image.h"
#include "chrome/install_static/install_util.h"
#include "components/browser_watcher/features.h"
#include "components/browser_watcher/stability_data_names.h"
#include "components/browser_watcher/stability_debugging.h"
#include "components/browser_watcher/stability_metrics.h"
#include "components/browser_watcher/stability_paths.h"
#include "components/browser_watcher/stability_report.pb.h"
#endif

#if defined(OS_WIN)
// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif

namespace chrome {

namespace {

void SetupStunProbeTrial() {
  std::map<std::string, std::string> params;
  if (!variations::GetVariationParams("StunProbeTrial2", &params))
    return;

  // The parameter, used by StartStunFieldTrial, should have the following
  // format: "request_per_ip/interval/sharedsocket/batch_size/total_batches/
  // server1:port/server2:port/server3:port/"
  std::string cmd_param = params["request_per_ip"] + "/" + params["interval"] +
                          "/" + params["sharedsocket"] + "/" +
                          params["batch_size"] + "/" + params["total_batches"] +
                          "/" + params["server1"] + "/" + params["server2"] +
                          "/" + params["server3"] + "/" + params["server4"] +
                          "/" + params["server5"] + "/" + params["server6"];

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWebRtcStunProbeTrialParameter, cmd_param);
}

#if defined(OS_WIN)

// Record information about the chrome module.
void RecordChromeModuleInfo(
    base::debug::GlobalActivityTracker* global_tracker) {
  DCHECK(global_tracker);

  base::debug::GlobalActivityTracker::ModuleInfo module;
  module.is_loaded = true;
  module.address = reinterpret_cast<uintptr_t>(&__ImageBase);

  base::win::PEImage pe(&__ImageBase);
  PIMAGE_NT_HEADERS headers = pe.GetNTHeaders();
  CHECK(headers);
  module.size = headers->OptionalHeader.SizeOfImage;
  module.timestamp = headers->FileHeader.TimeDateStamp;

  GUID guid;
  DWORD age;
  if (pe.GetDebugId(&guid, &age, /* pdb_filename= */ nullptr,
                    /* pdb_filename_length= */ nullptr)) {
    module.age = age;
    static_assert(sizeof(module.identifier) >= sizeof(guid),
                  "Identifier field must be able to contain a GUID.");
    memcpy(module.identifier, &guid, sizeof(guid));
  } else {
    memset(module.identifier, 0, sizeof(module.identifier));
  }

  module.file = "chrome.dll";
  module.debug_file = "chrome.dll.pdb";

  global_tracker->RecordModuleInfo(module);
}

void SetupStabilityDebugging() {
  if (!base::FeatureList::IsEnabled(
          browser_watcher::kStabilityDebuggingFeature)) {
    return;
  }

  // TODO(bcwhite): Adjust these numbers once there is real data to show
  // just how much of an arena is necessary.
  const size_t kMemorySize = 1 << 20;  // 1 MiB
  const int kStackDepth = 4;
  const uint64_t kAllocatorId = 0;

  // Ensure the stability directory exists and determine the stability file's
  // path.
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir) ||
      !base::CreateDirectory(browser_watcher::GetStabilityDir(user_data_dir))) {
    return;
  }
  browser_watcher::LogStabilityRecordEvent(
      browser_watcher::StabilityRecordEvent::kStabilityDirectoryExists);

  base::FilePath stability_file;
  if (!browser_watcher::GetStabilityFileForProcess(
          base::Process::Current(), user_data_dir, &stability_file)) {
    return;
  }
  browser_watcher::LogStabilityRecordEvent(
      browser_watcher::StabilityRecordEvent::kGotStabilityPath);

  // Track code activities (such as posting task, blocking on locks, and
  // joining threads) that can cause hanging threads and general instability
  base::debug::GlobalActivityTracker::CreateWithFile(
      stability_file, kMemorySize, kAllocatorId,
      browser_watcher::kStabilityDebuggingFeature.name, kStackDepth);

  // Record basic information.
  base::debug::GlobalActivityTracker* global_tracker =
      base::debug::GlobalActivityTracker::Get();
  if (global_tracker) {
    browser_watcher::LogStabilityRecordEvent(
        browser_watcher::StabilityRecordEvent::kGotTracker);
    // Record product, version, channel, special build and platform.
    wchar_t exe_file[MAX_PATH] = {};
    CHECK(::GetModuleFileName(nullptr, exe_file, base::size(exe_file)));

    base::string16 product_name;
    base::string16 version_number;
    base::string16 channel_name;
    base::string16 special_build;
    install_static::GetExecutableVersionDetails(exe_file, &product_name,
                                                &version_number, &special_build,
                                                &channel_name);

    base::debug::ActivityUserData& proc_data = global_tracker->process_data();
    proc_data.SetString(browser_watcher::kStabilityProduct, product_name);
    proc_data.SetString(browser_watcher::kStabilityVersion, version_number);
    proc_data.SetString(browser_watcher::kStabilityChannel, channel_name);
    proc_data.SetString(browser_watcher::kStabilitySpecialBuild, special_build);
#if defined(ARCH_CPU_X86)
    proc_data.SetString(browser_watcher::kStabilityPlatform, "Win32");
#elif defined(ARCH_CPU_X86_64)
    proc_data.SetString(browser_watcher::kStabilityPlatform, "Win64");
#endif
    proc_data.SetInt(browser_watcher::kStabilityStartTimestamp,
                     base::Time::Now().ToInternalValue());
    proc_data.SetInt(browser_watcher::kStabilityProcessType,
                     browser_watcher::ProcessState::BROWSER_PROCESS);

    // Record information about chrome's module. We want this to be done early.
    RecordChromeModuleInfo(global_tracker);

    // Trigger a flush of the memory mapped file to maximize the chances of
    // having a minimal amount of content in the stability file, even if
    // the system crashes or loses power. Even running in the background,
    // this is a potentially expensive operation, so done under an experiment
    // to allow measuring the performance effects, if any.
    const bool should_flush = base::GetFieldTrialParamByFeatureAsBool(
        browser_watcher::kStabilityDebuggingFeature,
        browser_watcher::kInitFlushParam, false);
    if (should_flush) {
      base::PostTask(
          FROM_HERE, {base::ThreadPool(), base::MayBlock()},
          base::BindOnce(&base::PersistentMemoryAllocator::Flush,
                         base::Unretained(global_tracker->allocator()), true));
    }

    // Store a copy of the system profile in this allocator. There will be some
    // delay before this gets populated, perhaps as much as a minute. Because
    // of this, there is no need to flush it here.
    metrics::GlobalPersistentSystemProfile::GetInstance()
        ->RegisterPersistentAllocator(global_tracker->allocator());

    browser_watcher::RegisterStabilityVEH();
  }
}
#endif  // defined(OS_WIN)

}  // namespace

void SetupDesktopFieldTrials() {
  prerender::ConfigureNoStatePrefetch();
  SetupStunProbeTrial();
#if defined(OS_WIN)
  SetupStabilityDebugging();
#endif  // defined(OS_WIN)
}

}  // namespace chrome
