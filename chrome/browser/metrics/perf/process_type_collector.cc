// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/process_type_collector.h"

#include "base/metrics/histogram_macros.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "content/public/common/content_switches.h"
#include "third_party/re2/src/re2/re2.h"

namespace metrics {

namespace {

// Name the histogram that represents the success and various failure modes for
// reporting collection of types.
const char kUmaHistogramName[] = "ChromeOS.CWP.CollectProcessTypes";

void SkipLine(std::string_view* contents) {
  static const LazyRE2 kSkipLine = {R"(.+\n?)"};
  RE2::Consume(contents, *kSkipLine);
}

// Matches both Ash-Chrome and Lacros binaries.
const LazyRE2 kChromeExePathMatcher = {
    R"((/opt/google/chrome/chrome|\S*lacros\S*/chrome))"};

// Matches Lacros binaries.
const LazyRE2 kLacrosExePathMatcher = {R"((\S*lacros\S*/chrome))"};
}  // namespace

std::map<uint32_t, Process> ProcessTypeCollector::ChromeProcessTypes(
    std::vector<uint32_t>& lacros_pids,
    std::string& lacros_path) {
  std::string output;
  if (!base::GetAppOutput(std::vector<std::string>({"ps", "-ewwo", "pid,cmd"}),
                          &output)) {
    UMA_HISTOGRAM_ENUMERATION(kUmaHistogramName,
                              CollectionAttemptStatus::kProcessTypeCmdError);
    return std::map<uint32_t, Process>();
  }

  return ParseProcessTypes(output, lacros_pids, lacros_path);
}

std::map<uint32_t, Thread> ProcessTypeCollector::ChromeThreadTypes() {
  std::string output;
  if (!base::GetAppOutput(
          std::vector<std::string>({"ps", "-ewwLo", "pid,lwp,comm,cmd"}),
          &output)) {
    UMA_HISTOGRAM_ENUMERATION(kUmaHistogramName,
                              CollectionAttemptStatus::kThreadTypeCmdError);
    return std::map<uint32_t, Thread>();
  }

  return ParseThreadTypes(output);
}

std::map<uint32_t, Process> ProcessTypeCollector::ParseProcessTypes(
    std::string_view contents,
    std::vector<uint32_t>& lacros_pids,
    std::string& lacros_path) {
  static const LazyRE2 kLineMatcher = {
      R"(\s*(\d+))"    // PID
      R"(\s+(.+)\n?)"  // COMMAND LINE
  };

  // Type flag with or without any value.
  const std::string kTypeFlagRegex =
      base::StrCat({R"(.*--)", switches::kProcessType, R"(=(\S*))"});

  static const LazyRE2 kTypeFlagMatcher = {kTypeFlagRegex.c_str()};

  // Skip header.
  SkipLine(&contents);

  std::map<uint32_t, Process> process_types;
  bool is_truncated = false;
  while (!contents.empty()) {
    uint32_t pid = 0;
    std::string_view cmd_line;
    if (!RE2::Consume(&contents, *kLineMatcher, &pid, &cmd_line)) {
      SkipLine(&contents);
      is_truncated = true;
      continue;
    }

    if (process_types.find(pid) != process_types.end()) {
      is_truncated = true;
      continue;
    }

    std::string_view cmd;
    if (!RE2::Consume(&cmd_line, *kChromeExePathMatcher, &cmd)) {
      continue;
    }

    // Use a second match to record any Lacros PID.
    std::string_view lacros_cmd;
    if (RE2::Consume(&cmd, *kLacrosExePathMatcher, &lacros_cmd)) {
      lacros_pids.emplace_back(pid);
      if (lacros_path.empty()) {
        lacros_path = lacros_cmd;
      }
    }

    std::string type;
    RE2::Consume(&cmd_line, *kTypeFlagMatcher, &type);

    Process process = Process::OTHER_PROCESS;
    if (type.empty()) {
      process = Process::BROWSER_PROCESS;
    } else if (type == switches::kRendererProcess) {
      process = Process::RENDERER_PROCESS;
    } else if (type == switches::kGpuProcess) {
      process = Process::GPU_PROCESS;
    } else if (type == switches::kUtilityProcess) {
      process = Process::UTILITY_PROCESS;
    } else if (type == switches::kZygoteProcess) {
      process = Process::ZYGOTE_PROCESS;
    } else if (type == switches::kPpapiPluginProcess) {
      process = Process::PPAPI_PLUGIN_PROCESS;
    }

    process_types.emplace(pid, process);
  }

  if (process_types.empty()) {
    UMA_HISTOGRAM_ENUMERATION(kUmaHistogramName,
                              CollectionAttemptStatus::kEmptyProcessType);
  } else if (is_truncated) {
    UMA_HISTOGRAM_ENUMERATION(kUmaHistogramName,
                              CollectionAttemptStatus::kProcessTypeTruncated);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kUmaHistogramName,
                              CollectionAttemptStatus::kProcessTypeSuccess);
  }
  return process_types;
}

std::map<uint32_t, Thread> ProcessTypeCollector::ParseThreadTypes(
    std::string_view contents) {
  static const LazyRE2 kLineMatcher = {
      R"(\s*(\d+))"    // PID
      R"(\s+(\d+))"    // TID
      R"(\s+(.+)\n?)"  // COMM and CMD, either of which may contain spaces
  };

  // Skip header.
  SkipLine(&contents);

  std::map<uint32_t, Thread> thread_types;
  bool is_truncated = false;
  while (!contents.empty()) {
    uint32_t pid = 0, tid = 0;
    std::string_view comm_cmd;
    if (!RE2::Consume(&contents, *kLineMatcher, &pid, &tid, &comm_cmd)) {
      SkipLine(&contents);
      is_truncated = true;
      continue;
    }

    if (!RE2::PartialMatch(comm_cmd, *kChromeExePathMatcher)) {
      continue;
    }

    if (thread_types.find(tid) != thread_types.end()) {
      is_truncated = true;
      continue;
    }

    Thread thread = Thread::OTHER_THREAD;
    if (pid == tid) {
      thread = Thread::MAIN_THREAD;
    } else if (comm_cmd.starts_with("Chrome_IOThread") ||
               comm_cmd.starts_with("Chrome_ChildIOT")) {
      thread = Thread::IO_THREAD;
    } else if (comm_cmd.starts_with("CompositorTileW")) {
      thread = Thread::COMPOSITOR_TILE_WORKER_THREAD;
    } else if (comm_cmd.starts_with("Compositor") ||
               comm_cmd.starts_with("VizCompositorTh")) {
      thread = Thread::COMPOSITOR_THREAD;
    } else if (comm_cmd.starts_with("ThreadPool")) {
      thread = Thread::THREAD_POOL_THREAD;
    } else if (comm_cmd.starts_with("DrmThread")) {
      thread = Thread::DRM_THREAD;
    } else if (comm_cmd.starts_with("GpuMemory")) {
      thread = Thread::GPU_MEMORY_THREAD;
    } else if (comm_cmd.starts_with("MemoryInfra")) {
      thread = Thread::MEMORY_INFRA_THREAD;
    } else if (comm_cmd.starts_with("Media")) {
      thread = Thread::MEDIA_THREAD;
    } else if (comm_cmd.starts_with("DedicatedWorker")) {
      thread = Thread::DEDICATED_WORKER_THREAD;
    } else if (comm_cmd.starts_with("ServiceWorker")) {
      thread = Thread::SERVICE_WORKER_THREAD;
    } else if (comm_cmd.starts_with("WebRTC")) {
      thread = Thread::WEBRTC_THREAD;
    } else if (comm_cmd.starts_with("dav1d-worker")) {
      thread = Thread::DAV1D_WORKER_THREAD;
    } else if (comm_cmd.starts_with("AudioThread")) {
      thread = Thread::AUDIO_THREAD;
    } else if (comm_cmd.starts_with("AudioOutputDevi")) {
      thread = Thread::AUDIO_DEVICE_THREAD;
    } else if (comm_cmd.starts_with("StackSamplingPr")) {
      thread = Thread::STACK_SAMPLING_THREAD;
    } else if (comm_cmd.starts_with("VideoFrameCompo")) {
      thread = Thread::VIDEO_FRAME_COMPOSITOR_THREAD;
    } else if (comm_cmd.starts_with("CodecWorker")) {
      thread = Thread::CODEC_WORKER_THREAD;
    }

    thread_types.emplace(tid, thread);
  }

  if (thread_types.empty()) {
    UMA_HISTOGRAM_ENUMERATION(kUmaHistogramName,
                              CollectionAttemptStatus::kEmptyThreadType);
  } else if (is_truncated) {
    UMA_HISTOGRAM_ENUMERATION(kUmaHistogramName,
                              CollectionAttemptStatus::kThreadTypeTruncated);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kUmaHistogramName,
                              CollectionAttemptStatus::kThreadTypeSuccess);
  }

  return thread_types;
}

}  // namespace metrics
