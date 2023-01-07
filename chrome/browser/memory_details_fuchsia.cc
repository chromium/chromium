// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"

#include <lib/zx/process.h>

#include "base/functional/bind.h"
#include "base/process/process_handle.h"
#include "chrome/grit/chromium_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

MemoryDetails::MemoryDetails() {
  ProcessData process_data;
  process_data.name = l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
  process_data.process_name = u"chrome";

  process_data_.push_back(process_data);
}

ProcessData* MemoryDetails::ChromeBrowser() {
  return &process_data_[0];
}

void MemoryDetails::CollectProcessData(
    const std::vector<ProcessMemoryInformation>& child_info) {
  process_data_[0].processes = child_info;

  for (auto& pmi : process_data_[0].processes) {
    pmi.num_processes = 1;
  }

  ProcessMemoryInformation browser_process;
  browser_process.num_processes = 1;
  browser_process.pid = base::GetCurrentProcId();
  browser_process.process_type = content::PROCESS_TYPE_BROWSER;
  process_data_[0].processes.push_back(std::move(browser_process));

  // Finally return to the browser thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MemoryDetails::CollectChildInfoOnUIThread, this));
}
