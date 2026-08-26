// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/chrome_browser_main_extra_parts_profiling.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiling_host/chrome_client_connection_manager.h"
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "chrome/common/chrome_switches.h"
#include "components/heap_profiling/multi_process/supervisor.h"
#include "components/services/heap_profiling/public/cpp/settings.h"

namespace {

std::unique_ptr<heap_profiling::ClientConnectionManager>
CreateClientConnectionManager(
    base::WeakPtr<heap_profiling::Controller> controller_weak_ptr,
    heap_profiling::Mode mode) {
  return std::make_unique<heap_profiling::ChromeClientConnectionManager>(
      controller_weak_ptr, mode);
}

}  // namespace

ChromeBrowserMainExtraPartsProfiling::ChromeBrowserMainExtraPartsProfiling() =
    default;
ChromeBrowserMainExtraPartsProfiling::~ChromeBrowserMainExtraPartsProfiling() =
    default;

void ChromeBrowserMainExtraPartsProfiling::PostCreateThreads() {
  heap_profiling::Supervisor::GetInstance()
      ->SetClientConnectionManagerConstructor(&CreateClientConnectionManager);

#if !defined(ADDRESS_SANITIZER)
  // Memory sanitizers are using large memory shadow to keep track of memory
  // state. Using memlog and memory sanitizers at the same time is slowing down
  // user experience, causing the browser to be barely responsive. In theory,
  // memlog and memory sanitizers are compatible and can run at the same time.
  heap_profiling::Mode mode = heap_profiling::GetModeForStartup();
  if (mode != heap_profiling::Mode::kNone) {
    heap_profiling::Supervisor::GetInstance()->Start(
        base::BindOnce(
            &heap_profiling::ProfilingProcessHost::Start,
            base::Unretained(
                heap_profiling::ProfilingProcessHost::GetInstance())));
  }
#endif
}
