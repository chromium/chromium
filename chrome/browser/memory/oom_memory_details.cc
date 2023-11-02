// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/oom_memory_details.h"

#include "base/logging.h"
#include "base/process/process_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/text/bytes_formatting.h"

namespace memory {

// static
void OomMemoryDetails::Log(const std::string& title) {
  // Deletes itself upon completion.
  OomMemoryDetails* details = new OomMemoryDetails(title);
  details->StartFetch();
}

OomMemoryDetails::OomMemoryDetails(const std::string& title)
    : title_(title) {
  AddRef();  // Released in OnDetailsAvailable().
  start_time_ = base::TimeTicks::Now();
}

OomMemoryDetails::~OomMemoryDetails() {
}

void OomMemoryDetails::OnDetailsAvailable() {
  base::TimeDelta delta = base::TimeTicks::Now() - start_time_;
  // These logs are collected by user feedback reports.  We want them to help
  // diagnose user-reported problems with frequently discarded tabs.
  std::string log_string = ToLogString(/*include_tab_title=*/false);
#if BUILDFLAG(IS_CHROMEOS)
  base::GraphicsMemoryInfoKB gpu_meminfo;
  if (base::GetGraphicsMemoryInfo(&gpu_meminfo)) {
    log_string +=
        "Graphics " +
        base::UTF16ToASCII(ui::FormatBytes(gpu_meminfo.gpu_memory_size));
  }
#endif
  LOG(WARNING) << title_ << " (" << delta.InMilliseconds() << " ms):\n"
               << log_string;
  // Delete ourselves so we don't have to worry about OomPriorityManager
  // deleting us when we're still working.
  Release();
}

}  // namespace memory
