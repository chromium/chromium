// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/browser/cfm_memory_details.h"

#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_set.h"

namespace ash::cfm {

// TODO(https://crbug.com/1403174): Remove when namespace of mojoms for CfM are
// migarted to ash.
namespace mojom = ::chromeos::cfm::mojom;

// static
void CfmMemoryDetails::Collect(
    mojom::CfmBrowser::GetMemoryDetailsCallback callback) {
  // Deletes itself upon completion.
  CfmMemoryDetails* details = new CfmMemoryDetails(std::move(callback));
  details->StartFetch();
}

CfmMemoryDetails::CfmMemoryDetails(
    mojom::CfmBrowser::GetMemoryDetailsCallback callback)
    : callback_(std::move(callback)) {
  AddRef();  // Released at the end of FinishFetch().
}

CfmMemoryDetails::~CfmMemoryDetails() = default;

void CfmMemoryDetails::OnDetailsAvailable() {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&CfmMemoryDetails::OnDetailsAvailable, this));
    return;
  }

  CollectProcessInformation();

  // Now Post to UI thread for expensive extensions information lookups.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CfmMemoryDetails::CollectExtensionsInformation, this));
}

void CfmMemoryDetails::CollectProcessInformation() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  proc_data_list_.reserve(processes().size());
  for (const ProcessData& proc_data : processes()) {
    std::vector<mojom::ProcessMemoryInformationPtr> proc_mem_info_list;
    proc_mem_info_list.reserve(proc_data.processes.size());
    for (const ProcessMemoryInformation& proc_mem_info : proc_data.processes) {
      auto proc_mem_info_mojom = mojom::ProcessMemoryInformation::New();
      proc_mem_info_mojom->pid = proc_mem_info.pid;
      proc_mem_info_mojom->version = base::UTF16ToUTF8(proc_mem_info.version);
      proc_mem_info_mojom->product_name =
          base::UTF16ToUTF8(proc_mem_info.product_name);
      proc_mem_info_mojom->num_processes = proc_mem_info.num_processes;
      proc_mem_info_mojom->num_open_fds = proc_mem_info.num_open_fds;
      proc_mem_info_mojom->open_fds_soft_limit =
          proc_mem_info.open_fds_soft_limit;
      proc_mem_info_mojom->private_memory_footprint_kb =
          proc_mem_info.private_memory_footprint_kb;

      // Avoid DCHECK in test builds by defining unknown directly
      bool process_type_unknown = proc_mem_info.process_type ==
                                  content::ProcessType::PROCESS_TYPE_UNKNOWN;
      bool process_type_renderer = proc_mem_info.process_type ==
                                   content::ProcessType::PROCESS_TYPE_RENDERER;
      bool renderer_type_unknown =
          proc_mem_info.renderer_type ==
          ProcessMemoryInformation::RendererProcessType::RENDERER_UNKNOWN;

      proc_mem_info_mojom->process_type =
          (process_type_renderer && renderer_type_unknown) ||
                  process_type_unknown
              ? "unknown"
              : ProcessMemoryInformation::GetFullTypeNameInEnglish(
                    proc_mem_info.process_type, proc_mem_info.renderer_type);

      proc_mem_info_mojom->renderer_type =
          renderer_type_unknown
              ? "unknown"
              : ProcessMemoryInformation::GetRendererTypeNameInEnglish(
                    proc_mem_info.renderer_type);

      auto& titles = proc_mem_info_mojom->titles;
      titles.reserve(proc_mem_info.titles.size());
      for (const std::u16string& title : proc_mem_info.titles) {
        titles.push_back(base::UTF16ToUTF8(title));
      }

      proc_mem_info_list.push_back(std::move(proc_mem_info_mojom));
      // We push back to a map that we can use to complete filling out
      // various information such as extensions
      proc_mem_info_map_[proc_mem_info.pid] = proc_mem_info_list.back().get();
    }
    proc_data_list_.push_back(
        mojom::ProcessData::New(base::UTF16ToUTF8(proc_data.name),
                                base::UTF16ToUTF8(proc_data.process_name),
                                std::move(proc_mem_info_list)));
  }
}

void CfmMemoryDetails::CollectExtensionsInformation() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  for (content::RenderProcessHost::iterator it(
           content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    content::RenderProcessHost* host = it.GetCurrentValue();
    // Only add valid processes
    if (!host->GetProcess().IsValid()) {
      continue;
    }

    // Check if process was added in current list
    auto* proc_mem_info =
        base::FindPtrOrNull(proc_mem_info_map_, host->GetProcess().Pid());
    if (!proc_mem_info) {
      continue;
    }

    // If no extension can be found in this process then no more work is
    // needed
    if (const extensions::Extension* extension =
            extensions::ProcessMap::Get(host->GetBrowserContext())
                ->GetEnabledExtensionByProcessID(host->GetID())) {
      proc_mem_info->extension_info.push_back(mojom::ExtensionData::New(
          extension->name(), extension->GetVersionForDisplay(), extension->id(),
          extension->hashed_id().value(), extension->description()));
    }
  }
#endif

  FinishFetch();
}

void CfmMemoryDetails::UpdateGpuInfo() {
  // Chrome OS exposes system-wide graphics driver memory which has
  // historically been a source of leak/bloat.
  base::GetGraphicsMemoryInfo(&gpu_meminfo_);
}

void CfmMemoryDetails::FinishFetch() {
  UpdateGpuInfo();
  std::move(callback_).Run(std::move(proc_data_list_),
                           gpu_meminfo_.gpu_memory_size);

  // Cleanup to ensure we are correctly released from memory.
  Release();
}

}  // namespace ash::cfm
