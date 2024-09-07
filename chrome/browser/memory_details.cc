// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"

#include <algorithm>
#include <set>
#include <vector>

#include "base/containers/adapters.h"
#include "base/file_version_info.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/nacl/common/nacl_process_type.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
#include "content/public/browser/zygote_host/zygote_host_linux.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/view_type.mojom.h"
#endif

using base::StringPrintf;
using content::BrowserChildProcessHostIterator;
using content::BrowserThread;
using content::NavigationEntry;
using content::RenderWidgetHost;
using content::WebContents;
#if BUILDFLAG(ENABLE_EXTENSIONS)
using extensions::Extension;
#endif

namespace {

void UpdateProcessTypeAndTitles(
#if BUILDFLAG(ENABLE_EXTENSIONS)
    const extensions::ExtensionSet* extension_set,
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
    ProcessMemoryInformation& process,
    content::RenderFrameHost* rfh) {
  // We check the title and the renderer type only of the primary main
  // RenderFrameHost, not subframes or non-primary main RenderFrameHosts. It is
  // OK because this logic is used to get the title and the renderer type only
  // for chrome://system and for printing the details to the error log when the
  // tab is oom-killed.
  if (!rfh->IsInPrimaryMainFrame())
    return;

  WebContents* contents = WebContents::FromRenderFrameHost(rfh);
  DCHECK(contents);

  // The rest of this block will happen only once per WebContents.
  GURL page_url = contents->GetLastCommittedURL();
  bool is_webui =
      rfh->GetEnabledBindings().Has(content::BindingsPolicyValue::kWebUi);

  if (is_webui) {
    process.renderer_type = ProcessMemoryInformation::RENDERER_CHROME;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!is_webui && extension_set) {
    const Extension* extension = extension_set->GetByID(page_url.host());
    if (extension) {
      process.titles.push_back(base::UTF8ToUTF16(extension->name()));
      process.renderer_type = ProcessMemoryInformation::RENDERER_EXTENSION;
      return;
    }
  }

  extensions::mojom::ViewType type = extensions::GetViewType(contents);
  if (type == extensions::mojom::ViewType::kBackgroundContents) {
    process.titles.push_back(base::UTF8ToUTF16(page_url.spec()));
    process.renderer_type = ProcessMemoryInformation::RENDERER_BACKGROUND_APP;
    return;
  }
#endif

  std::u16string title = contents->GetTitle();
  if (!title.length())
    title = l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
  process.titles.push_back(title);
}

}  // namespace

// static
std::string ProcessMemoryInformation::GetRendererTypeNameInEnglish(
    RendererProcessType type) {
  switch (type) {
    case RENDERER_NORMAL:
      return "Tab";
    case RENDERER_CHROME:
      return "Tab (Chrome)";
    case RENDERER_EXTENSION:
      return "Extension";
    case RENDERER_DEVTOOLS:
      return "Devtools";
    case RENDERER_INTERSTITIAL:
      return "Interstitial";
    case RENDERER_BACKGROUND_APP:
      return "Background App";
    case RENDERER_UNKNOWN:
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown renderer process type!";
      return "Unknown";
  }
}

// static
std::string ProcessMemoryInformation::GetFullTypeNameInEnglish(
    int process_type,
    RendererProcessType rtype) {
  if (process_type == content::PROCESS_TYPE_RENDERER)
    return GetRendererTypeNameInEnglish(rtype);
  return content::GetProcessTypeNameInEnglish(process_type);
}

ProcessMemoryInformation::ProcessMemoryInformation()
    : pid(0),
      num_processes(0),
      process_type(content::PROCESS_TYPE_UNKNOWN),
      num_open_fds(-1),
      open_fds_soft_limit(-1),
      renderer_type(RENDERER_UNKNOWN),
      private_memory_footprint_kb(0) {}

ProcessMemoryInformation::ProcessMemoryInformation(
    const ProcessMemoryInformation& other) = default;

ProcessMemoryInformation::~ProcessMemoryInformation() {}

bool ProcessMemoryInformation::operator<(
    const ProcessMemoryInformation& rhs) const {
  return private_memory_footprint_kb < rhs.private_memory_footprint_kb;
}

ProcessData::ProcessData() {}

ProcessData::ProcessData(const ProcessData& rhs)
    : name(rhs.name),
      process_name(rhs.process_name),
      processes(rhs.processes) {
}

ProcessData::~ProcessData() {}

ProcessData& ProcessData::operator=(const ProcessData& rhs) {
  name = rhs.name;
  process_name = rhs.process_name;
  processes = rhs.processes;
  return *this;
}

// This operation can take 30-100ms to complete.  We never want to have
// one task run for that long on the UI or IO threads.  So, we run the
// expensive parts of this operation over on the blocking pool.
void MemoryDetails::StartFetch() {
  // This might get called from the UI or FILE threads, but should not be
  // getting called from the IO thread.
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));

  std::vector<ProcessMemoryInformation> child_info;

  // Collect the list of child processes. A 0 |handle| means that
  // the process is being launched, so we skip it.
  for (BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    ProcessMemoryInformation info;
    if (!iter.GetData().GetProcess().IsValid())
      continue;
    info.pid = iter.GetData().GetProcess().Pid();
    if (!info.pid)
      continue;

    info.process_type = iter.GetData().process_type;
    info.renderer_type = ProcessMemoryInformation::RENDERER_UNKNOWN;
    info.titles.push_back(iter.GetData().name);
    child_info.push_back(info);
  }

  // Now go do expensive memory lookups in a thread pool.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&MemoryDetails::CollectProcessData, this, child_info));
}

MemoryDetails::~MemoryDetails() {}

std::string MemoryDetails::ToLogString(bool include_tab_title) {
  std::string log;
  log.reserve(4096);
  ProcessMemoryInformationList processes = ChromeBrowser()->processes;
  // Sort by memory consumption, low to high.
  std::sort(processes.begin(), processes.end());
  // Print from high to low.
  for (const ProcessMemoryInformation& process_info :
       base::Reversed(processes)) {
    log += ProcessMemoryInformation::GetFullTypeNameInEnglish(
        process_info.process_type, process_info.renderer_type);
    // The title of a renderer may contain PII.
    if ((process_info.process_type != content::PROCESS_TYPE_RENDERER ||
         include_tab_title) &&
        !process_info.titles.empty()) {
      log += " [";
      bool first_title = true;
      for (const std::u16string& title : process_info.titles) {
        if (!first_title)
          log += "|";
        first_title = false;
        log += base::UTF16ToUTF8(title);
      }
      log += "]";
    }
    log += StringPrintf(
        " %d MB",
        static_cast<int>(process_info.private_memory_footprint_kb) / 1024);
    if (process_info.num_open_fds != -1 ||
        process_info.open_fds_soft_limit != -1) {
      log += StringPrintf(", %d FDs open of %d", process_info.num_open_fds,
                          process_info.open_fds_soft_limit);
    }
    log += "\n";
  }
  return log;
}

void MemoryDetails::CollectChildInfoOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ProcessData* const chrome_browser = ChromeBrowser();

  // First pass, collate the widgets by process ID.
  std::map<base::ProcessId, std::vector<RenderWidgetHost*>> widgets_by_pid;
  std::unique_ptr<content::RenderWidgetHostIterator> widget_it(
      RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widget_it->GetNextHost()) {
    // Ignore processes that don't have a connection, such as crashed tabs,
    // or processes that are still launching.
    if (!widget->GetProcess()->IsReady())
      continue;
    base::ProcessId pid = widget->GetProcess()->GetProcess().Pid();
    widgets_by_pid[pid].push_back(widget);
  }

  // Get more information about the process.
  for (ProcessMemoryInformation& process : chrome_browser->processes) {
    // If there's at least one widget in the process, it is some kind of
    // renderer process belonging to this browser. All these widgets will share
    // a RenderProcessHost.
    content::RenderProcessHost* render_process_host = nullptr;
    if (!widgets_by_pid[process.pid].empty()) {
      // Mark it as a normal renderer process, if we don't refine it to some
      // other |renderer_type| later.
      process.process_type = content::PROCESS_TYPE_RENDERER;
      process.renderer_type = ProcessMemoryInformation::RENDERER_NORMAL;
      render_process_host = widgets_by_pid[process.pid].front()->GetProcess();
    }

#if BUILDFLAG(ENABLE_EXTENSIONS)
    // Determine if this is an extension process.
    bool process_is_for_extensions = false;
    const extensions::ExtensionSet* extension_set = nullptr;
    if (render_process_host &&
        !extensions::ChromeContentBrowserClientExtensionsPart::
            AreExtensionsDisabledForProfile(
                render_process_host->GetBrowserContext())) {
      content::BrowserContext* context =
          render_process_host->GetBrowserContext();
      extensions::ExtensionRegistry* extension_registry =
          extensions::ExtensionRegistry::Get(context);
      DCHECK(extension_registry);
      extension_set = &extension_registry->enabled_extensions();
      extensions::ProcessMap* process_map =
          extensions::ProcessMap::Get(context);
      DCHECK(process_map);
      int rph_id = render_process_host->GetID();
      process_is_for_extensions = process_map->Contains(rph_id);

      // For our purposes, don't count processes running hosted apps as
      // extension processes. See also: crbug.com/102533.
      if (const Extension* extension =
              process_map->GetEnabledExtensionByProcessID(rph_id)) {
        if (!extension->is_hosted_app()) {
          process.renderer_type = ProcessMemoryInformation::RENDERER_EXTENSION;
        }
      }
    }
#endif

    if (render_process_host) {
      // Use the list of RenderFrameHosts to iterate over the WebContents
      // instances whose primary main RenderFrameHosts are in `process`. Refine
      // our determination of the `process.renderer_type`, and record the page
      // titles.
      render_process_host->ForEachRenderFrameHost(
          [&](content::RenderFrameHost* frame) {
            UpdateProcessTypeAndTitles(
#if BUILDFLAG(ENABLE_EXTENSIONS)
                process_is_for_extensions ? extension_set : nullptr,
#endif
                process, frame);
          });
    }

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
    if (content::ZygoteHost::GetInstance()->IsZygotePid(process.pid)) {
      process.process_type = content::PROCESS_TYPE_ZYGOTE;
    }
#endif
  }

  // Get rid of other Chrome processes that are from a different profile.
  auto is_unknown = [](ProcessMemoryInformation& process) {
    return process.process_type == content::PROCESS_TYPE_UNKNOWN;
  };
  auto& vector = chrome_browser->processes;
  std::erase_if(vector, is_unknown);

  // Grab a memory dump for all processes.
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestPrivateMemoryFootprint(
          base::kNullProcessId,
          base::BindOnce(&MemoryDetails::DidReceiveMemoryDump, this));
}

void MemoryDetails::DidReceiveMemoryDump(
    bool success,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> global_dump) {
  ProcessData* const chrome_browser = ChromeBrowser();
  if (success) {
    for (const memory_instrumentation::GlobalMemoryDump::ProcessDump& dump :
         global_dump->process_dumps()) {
      base::ProcessId dump_pid = dump.pid();
      for (ProcessMemoryInformation& pmi : chrome_browser->processes) {
        if (pmi.pid == dump_pid) {
          pmi.private_memory_footprint_kb = dump.os_dump().private_footprint_kb;
          break;
        }
      }
    }
  }

  OnDetailsAvailable();
}
