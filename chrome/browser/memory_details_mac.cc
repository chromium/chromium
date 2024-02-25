// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>

#include "base/apple/foundation_util.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/process/process_iterator.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/process_type.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// A helper for |CollectProcessData()|, collecting data on the Chrome/Chromium
// process with PID |pid|. The collected data is added to |processes|.
void CollectProcessDataForChromeProcess(
    const std::vector<ProcessMemoryInformation>& child_info,
    base::ProcessId pid,
    ProcessMemoryInformationList* processes) {
  ProcessMemoryInformation info;
  info.pid = pid;
  if (info.pid == base::GetCurrentProcId())
    info.process_type = content::PROCESS_TYPE_BROWSER;
  else
    info.process_type = content::PROCESS_TYPE_UNKNOWN;

  info.product_name = base::ASCIIToUTF16(version_info::GetProductName());
  info.version = base::ASCIIToUTF16(version_info::GetVersionNumber());

  // A PortProvider is not necessary to acquire information about the number
  // of open file descriptors.
  std::unique_ptr<base::ProcessMetrics> metrics(
      base::ProcessMetrics::CreateProcessMetrics(pid, nullptr));
  info.num_open_fds = metrics->GetOpenFdCount();
  info.open_fds_soft_limit = metrics->GetOpenFdSoftLimit();

  // Check if this is one of the child processes whose data was already
  // collected and exists in |child_data|.
  for (const ProcessMemoryInformation& child : child_info) {
    if (child.pid == info.pid) {
      info.titles = child.titles;
      info.process_type = child.process_type;
      break;
    }
  }

  processes->push_back(info);
}

}  // namespace

MemoryDetails::MemoryDetails() {
  const base::FilePath browser_process_path =
      base::GetProcessExecutablePath(base::GetCurrentProcessHandle());

  ProcessData process;
  process.name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  process.process_name =
      base::UTF8ToUTF16(browser_process_path.BaseName().value());
  process_data_.push_back(process);
}

ProcessData* MemoryDetails::ChromeBrowser() {
  return &process_data_[0];
}

void MemoryDetails::CollectProcessData(
    const std::vector<ProcessMemoryInformation>& child_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Clear old data.
  process_data_[0].processes.clear();

  // First, we use |NamedProcessIterator| to get the PIDs of the processes we're
  // interested in; we save our results to avoid extra calls to
  // |NamedProcessIterator| (for performance reasons) and to avoid additional
  // inconsistencies caused by racing. Then we run |/bin/ps| *once* to get
  // information on those PIDs. Then we used our saved information to iterate
  // over browsers, then over PIDs.

  // Get PIDs of main browser processes.
  std::vector<base::ProcessId> all_pids;
  {
    base::NamedProcessIterator process_it(
        base::UTF16ToUTF8(process_data_[0].process_name), NULL);

    while (const base::ProcessEntry* entry = process_it.NextProcessEntry()) {
      all_pids.push_back(entry->pid());
    }
  }

  // Get PIDs of the helper.
  {
    base::NamedProcessIterator helper_it(chrome::kHelperProcessExecutableName,
                                         NULL, /*use_prefix_match=*/true);
    while (const base::ProcessEntry* entry = helper_it.NextProcessEntry()) {
      all_pids.push_back(entry->pid());
    }
  }

  ProcessMemoryInformationList* chrome_processes = &process_data_[0].processes;

  // Collect data about Chrome/Chromium.
  for (const base::ProcessId& pid : all_pids)
    CollectProcessDataForChromeProcess(child_info, pid, chrome_processes);

  // Finally return to the browser thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MemoryDetails::CollectChildInfoOnUIThread, this));
}
