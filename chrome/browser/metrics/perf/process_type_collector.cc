// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/process_type_collector.h"

#include "base/metrics/histogram_macros.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "content/public/common/content_switches.h"
#include "services/service_manager/embedder/switches.h"
#include "third_party/re2/src/re2/re2.h"

namespace metrics {

namespace {

// Name the histogram that represents the success and various failure modes for
// reporting collection of types.
const char kUmaHistogramName[] = "ChromeOS.CWP.CollectProcessTypes";

void SkipLine(re2::StringPiece* contents) {
  static const LazyRE2 kSkipLine = {R"(.+\n?)"};
  RE2::Consume(contents, *kSkipLine);
}

const LazyRE2 kChromeExePathMatcher = {R"(/opt/google/chrome/chrome\s*)"};

}  // namespace

std::map<uint32_t, Process> ProcessTypeCollector::ChromeProcessTypes() {
  std::string output;
  if (!base::GetAppOutput(std::vector<std::string>({"ps", "-ewwo", "pid,cmd"}),
                          &output)) {
    UMA_HISTOGRAM_ENUMERATION(kUmaHistogramName,
                              CollectionAttemptStatus::kProcessTypeCmdError);
    return std::map<uint32_t, Process>();
  }

  return ParseProcessTypes(output);
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
    re2::StringPiece contents) {
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
    re2::StringPiece cmd_line;
    if (!RE2::Consume(&contents, *kLineMatcher, &pid, &cmd_line)) {
      SkipLine(&contents);
      is_truncated = true;
      continue;
    }

    if (process_types.find(pid) != process_types.end()) {
      is_truncated = true;
      continue;
    }

    if (!RE2::Consume(&cmd_line, *kChromeExePathMatcher)) {
      continue;
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
    } else if (type == service_manager::switches::kZygoteProcess) {
      process = Process::ZYGOTE_PROCESS;
    } else if (type == switches::kPpapiPluginProcess) {
      process = Process::PPAPI_PLUGIN_PROCESS;
    } else if (type == switches::kPpapiBrokerProcess) {
      process = Process::PPAPI_BROKER_PROCESS;
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
    re2::StringPiece contents) {
  static const LazyRE2 kLineMatcher = {
      R"(\s*(\d+))"    // PID
      R"(\s+(\d+))"    // TID
      R"(\s+(\S+))"    // CMD
      R"(\s+(.+)\n?)"  // COMMAND LINE
  };

  // Skip header.
  SkipLine(&contents);

  std::map<uint32_t, Thread> thread_types;
  bool is_truncated = false;
  while (!contents.empty()) {
    uint32_t pid = 0, tid = 0;
    std::string cmd;
    re2::StringPiece cmd_line;
    if (!RE2::Consume(&contents, *kLineMatcher, &pid, &tid, &cmd, &cmd_line)) {
      SkipLine(&contents);
      is_truncated = true;
      continue;
    }

    if (!RE2::Consume(&cmd_line, *kChromeExePathMatcher)) {
      continue;
    }

    if (thread_types.find(tid) != thread_types.end()) {
      is_truncated = true;
      continue;
    }

    Thread thread = Thread::OTHER_THREAD;
    if (pid == tid) {
      thread = Thread::MAIN_THREAD;
    } else if (cmd == "Chrome_IOThread" ||
               base::StartsWith(cmd, "Chrome_ChildIOT",
                                base::CompareCase::SENSITIVE)) {
      thread = Thread::IO_THREAD;
    } else if (base::StartsWith(cmd, "CompositorTileW",
                                base::CompareCase::SENSITIVE)) {
      thread = Thread::COMPOSITOR_TILE_WORKER_THREAD;
    } else if (base::StartsWith(cmd, "Compositor",
                                base::CompareCase::SENSITIVE) ||
               base::StartsWith(cmd, "VizCompositorTh",
                                base::CompareCase::SENSITIVE)) {
      thread = Thread::COMPOSITOR_THREAD;
    } else if (base::StartsWith(cmd, "ThreadPool",
                                base::CompareCase::SENSITIVE)) {
      thread = Thread::THREAD_POOL_THREAD;
    } else if (base::StartsWith(cmd, "GpuMemory",
                                base::CompareCase::SENSITIVE)) {
      thread = Thread::GPU_MEMORY_THREAD;
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
