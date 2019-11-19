// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Windows headers must come first.
#include <windows.h>

#include <psapi.h>
#include <stddef.h>
#include <TlHelp32.h>

#include "chrome/browser/memory_details.h"

#include <memory>

#include "base/bind.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/process_type.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

MemoryDetails::MemoryDetails() {
  base::FilePath browser_process_path;
  base::PathService::Get(base::FILE_EXE, &browser_process_path);

  ProcessData process;
  process.name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  process.process_name = browser_process_path.BaseName().value();
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

  base::win::ScopedHandle snapshot(
      ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  PROCESSENTRY32 process_entry = {sizeof(PROCESSENTRY32)};
  if (!snapshot.Get()) {
    LOG(ERROR) << "CreateToolhelp32Snapshot failed: " << GetLastError();
    return;
  }
  if (!::Process32First(snapshot.Get(), &process_entry)) {
    LOG(ERROR) << "Process32First failed: " << GetLastError();
    return;
  }
  do {
    base::ProcessId pid = process_entry.th32ProcessID;
    base::win::ScopedHandle process_handle(::OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid));
    if (!process_handle.IsValid())
      continue;
    if (_wcsicmp(process_data_[0].process_name.c_str(),
                 process_entry.szExeFile) != 0) {
      continue;
    }

    // Get Memory Information.
    ProcessMemoryInformation info;
    info.pid = pid;
    info.process_type = pid == GetCurrentProcessId()
                            ? content::PROCESS_TYPE_BROWSER
                            : content::PROCESS_TYPE_UNKNOWN;

    // Get Version Information.
    info.version = base::ASCIIToUTF16(version_info::GetVersionNumber());
    // Check if this is one of the child processes whose data we collected
    // on the IO thread, and if so copy over that data.
    for (const ProcessMemoryInformation& child : child_info) {
      if (child.pid == info.pid) {
        info.titles = child.titles;
        info.process_type = child.process_type;
        break;
      }
    }

    // Add the process info to our list.
    process_data_[0].processes.push_back(info);
  } while (::Process32Next(snapshot.Get(), &process_entry));

  // Finally return to the browser thread.
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&MemoryDetails::CollectChildInfoOnUIThread, this));
}
