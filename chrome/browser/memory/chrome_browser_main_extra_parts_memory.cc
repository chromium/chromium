// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/chrome_browser_main_extra_parts_memory.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/memory_pressure_monitor.h"
#include "build/build_config.h"
#include "components/heap_profiling/in_process/browser_process_snapshot_controller.h"
#include "components/heap_profiling/in_process/mojom/snapshot_controller.mojom.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/components/memory/pressure/system_memory_pressure_evaluator.h"
#endif

namespace {

// A shim to connect HeapProfilerController, which can't depend on content/, to
// ChildProcessHost.
void BindHeapSnapshotControllerToProcessHost(
    int child_process_id,
    mojo::PendingReceiver<heap_profiling::mojom::SnapshotController> receiver) {
  // `child_process_id` could refer to a BrowserChildProcessHost or
  // RenderProcessHost.
  if (auto* bcph = content::BrowserChildProcessHost::FromID(child_process_id)) {
    bcph->GetHost()->BindReceiver(std::move(receiver));
  } else if (auto* rph = content::RenderProcessHost::FromID(child_process_id)) {
    if (!rph->GetBrowserContext()->IsOffTheRecord()) {
      rph->BindReceiver(std::move(receiver));
    }
  }
}

}  // namespace

ChromeBrowserMainExtraPartsMemory::ChromeBrowserMainExtraPartsMemory() = default;

ChromeBrowserMainExtraPartsMemory::~ChromeBrowserMainExtraPartsMemory() =
    default;

void ChromeBrowserMainExtraPartsMemory::PostCreateThreads() {
  // BrowserProcessSnapshotController may be null if heap profiling isn't
  // enabled in this session, or if the kHeapProfilerCentralControl feature is
  // disabled.
  if (auto* snapshot_controller =
          heap_profiling::BrowserProcessSnapshotController::GetInstance()) {
    snapshot_controller->SetBindRemoteForChildProcessCallback(
        base::BindRepeating(&BindHeapSnapshotControllerToProcessHost));
  }
}

void ChromeBrowserMainExtraPartsMemory::PostBrowserStart() {
  // The MemoryPressureMonitor might not be available in some tests.
  if (base::MemoryPressureMonitor::Get()) {
#if BUILDFLAG(IS_CHROMEOS)
    if (base::SysInfo::IsRunningOnChromeOS()) {
      cros_evaluator_ =
          std::make_unique<ash::memory::SystemMemoryPressureEvaluator>(
              static_cast<memory_pressure::MultiSourceMemoryPressureMonitor*>(
                  base::MemoryPressureMonitor::Get())
                  ->CreateVoter());
    }
#endif
  }
}

void ChromeBrowserMainExtraPartsMemory::PostMainMessageLoopRun() {
#if BUILDFLAG(IS_CHROMEOS)
  cros_evaluator_.reset();
#endif
}
