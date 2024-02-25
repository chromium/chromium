// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"

#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>

#include <map>
#include <memory>
#include <set>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/process/process_iterator.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/grit/branded_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/process_type.h"
#include "ui/base/l10n/l10n_util.h"

using base::ProcessEntry;
namespace {

struct Process {
  pid_t pid;
  pid_t parent;
};

typedef std::map<pid_t, Process> ProcessMap;

// Get information on all the processes running on the system.
ProcessMap GetProcesses() {
  ProcessMap map;

  base::ProcessIterator process_iter(nullptr);
  while (const ProcessEntry* process_entry = process_iter.NextProcessEntry()) {
    Process process;
    process.pid = process_entry->pid();
    process.parent = process_entry->parent_pid();
    map.insert(std::make_pair(process.pid, process));
  }
  return map;
}

// For each of a list of pids, collect memory information about that process.
ProcessData GetProcessDataMemoryInformation(
    const std::vector<pid_t>& pids) {
  ProcessData process_data;
  for (pid_t pid : pids) {
    ProcessMemoryInformation pmi;

    pmi.pid = pid;
    pmi.num_processes = 1;

    if (pmi.pid == base::GetCurrentProcId())
      pmi.process_type = content::PROCESS_TYPE_BROWSER;
    else
      pmi.process_type = content::PROCESS_TYPE_UNKNOWN;

    std::unique_ptr<base::ProcessMetrics> metrics(
        base::ProcessMetrics::CreateProcessMetrics(pid));
    pmi.num_open_fds = metrics->GetOpenFdCount();
    pmi.open_fds_soft_limit = metrics->GetOpenFdSoftLimit();

    process_data.processes.push_back(pmi);
  }
  return process_data;
}

// Find all children of the given process with pid |root|.
std::vector<pid_t> GetAllChildren(const ProcessMap& processes, pid_t root) {
  std::vector<pid_t> children;
  children.push_back(root);

  std::set<pid_t> wavefront, next_wavefront;
  wavefront.insert(root);

  while (wavefront.size()) {
    for (const auto& entry : processes) {
      const Process& process = entry.second;
      if (wavefront.count(process.parent)) {
        children.push_back(process.pid);
        next_wavefront.insert(process.pid);
      }
    }

    wavefront.clear();
    wavefront.swap(next_wavefront);
  }
  return children;
}

}  // namespace

MemoryDetails::MemoryDetails() {
}

ProcessData* MemoryDetails::ChromeBrowser() {
  return &process_data_[0];
}

void MemoryDetails::CollectProcessData(
    const std::vector<ProcessMemoryInformation>& child_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  ProcessMap process_map = GetProcesses();
  std::set<pid_t> browsers_found;

  ProcessData current_browser =
      GetProcessDataMemoryInformation(GetAllChildren(process_map, getpid()));
  current_browser.name = l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
  current_browser.process_name = u"chrome";

  for (auto i = current_browser.processes.begin();
       i != current_browser.processes.end(); ++i) {
    // Check if this is one of the child processes whose data we collected
    // on the IO thread, and if so copy over that data.
    for (size_t child = 0; child < child_info.size(); child++) {
      if (child_info[child].pid != i->pid)
        continue;
      i->titles = child_info[child].titles;
      i->process_type = child_info[child].process_type;
      break;
    }
  }

  process_data_.push_back(current_browser);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::GetSwapInfo(&swap_info_);
#endif

  // Finally return to the browser thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MemoryDetails::CollectChildInfoOnUIThread, this));
}
