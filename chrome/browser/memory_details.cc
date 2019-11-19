// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/file_version_info.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
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
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
#include "services/service_manager/zygote/zygote_host_linux.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#endif

using base::StringPrintf;
using content::BrowserChildProcessHostIterator;
using content::BrowserThread;
using content::NavigationEntry;
using content::RenderViewHost;
using content::RenderWidgetHost;
using content::WebContents;
#if BUILDFLAG(ENABLE_EXTENSIONS)
using extensions::Extension;
#endif

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
      NOTREACHED() << "Unknown renderer process type!";
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

// About threading:
//
// This operation will hit no fewer than 3 threads.
//
// The BrowserChildProcessHostIterator can only be accessed from the IO thread.
//
// The RenderProcessHostIterator can only be accessed from the UI thread.
//
// This operation can take 30-100ms to complete.  We never want to have
// one task run for that long on the UI or IO threads.  So, we run the
// expensive parts of this operation over on the blocking pool.
//
void MemoryDetails::StartFetch() {
  // This might get called from the UI or FILE threads, but should not be
  // getting called from the IO thread.
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));

  // In order to process this request, we need to use the plugin information.
  // However, plugin process information is only available from the IO thread.
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&MemoryDetails::CollectChildInfoOnIOThread, this));
}

MemoryDetails::~MemoryDetails() {}

std::string MemoryDetails::ToLogString() {
  std::string log;
  log.reserve(4096);
  ProcessMemoryInformationList processes = ChromeBrowser()->processes;
  // Sort by memory consumption, low to high.
  std::sort(processes.begin(), processes.end());
  // Print from high to low.
  for (auto iter1 = processes.rbegin(); iter1 != processes.rend(); ++iter1) {
    log += ProcessMemoryInformation::GetFullTypeNameInEnglish(
            iter1->process_type, iter1->renderer_type);
    if (!iter1->titles.empty()) {
      log += " [";
      for (std::vector<base::string16>::const_iterator iter2 =
               iter1->titles.begin();
           iter2 != iter1->titles.end(); ++iter2) {
        if (iter2 != iter1->titles.begin())
          log += "|";
        log += base::UTF16ToUTF8(*iter2);
      }
      log += "]";
    }
    log += StringPrintf(
        " %d MB", static_cast<int>(iter1->private_memory_footprint_kb) / 1024);
    if (iter1->num_open_fds != -1 || iter1->open_fds_soft_limit != -1) {
      log += StringPrintf(", %d FDs open of %d", iter1->num_open_fds,
                          iter1->open_fds_soft_limit);
    }
    log += "\n";
  }
  return log;
}

void MemoryDetails::CollectChildInfoOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

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
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&MemoryDetails::CollectProcessData, this, child_info));
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
    if (render_process_host) {
      content::BrowserContext* context =
          render_process_host->GetBrowserContext();
      extensions::ExtensionRegistry* extension_registry =
          extensions::ExtensionRegistry::Get(context);
      extensions::ProcessMap* process_map =
          extensions::ProcessMap::Get(context);
      int rph_id = render_process_host->GetID();
      process_is_for_extensions = process_map->Contains(rph_id);

      // For our purposes, don't count processes containing only hosted apps
      // as extension processes. See also: crbug.com/102533.
      for (auto& extension_id : process_map->GetExtensionsInProcess(rph_id)) {
        const Extension* extension =
            extension_registry->enabled_extensions().GetByID(extension_id);
        if (extension && !extension->is_hosted_app()) {
          process.renderer_type = ProcessMemoryInformation::RENDERER_EXTENSION;
          break;
        }
      }
    }
#endif

    // Use the list of widgets to iterate over the WebContents instances whose
    // main RenderFrameHosts are in |process|. Refine our determination of the
    // |process.renderer_type|, and record the page titles.
    for (content::RenderWidgetHost* widget : widgets_by_pid[process.pid]) {
      DCHECK_EQ(render_process_host, widget->GetProcess());

      RenderViewHost* rvh = RenderViewHost::From(widget);
      if (!rvh)
        continue;

      WebContents* contents = WebContents::FromRenderViewHost(rvh);

      // Assume that an RVH without a web contents is an interstitial.
      if (!contents) {
        process.renderer_type = ProcessMemoryInformation::RENDERER_INTERSTITIAL;
        continue;
      }

      // If this is a RVH for a subframe; skip it to avoid double-counting the
      // WebContents.
      if (rvh != contents->GetRenderViewHost())
        continue;

      // The rest of this block will happen only once per WebContents.
      GURL page_url = contents->GetLastCommittedURL();
      bool is_webui = rvh->GetMainFrame()->GetEnabledBindings() &
                      content::BINDINGS_POLICY_WEB_UI;

      if (is_webui) {
        process.renderer_type = ProcessMemoryInformation::RENDERER_CHROME;
      }

#if BUILDFLAG(ENABLE_EXTENSIONS)
      if (!is_webui && process_is_for_extensions) {
        const Extension* extension =
            extensions::ExtensionRegistry::Get(
                render_process_host->GetBrowserContext())
                ->enabled_extensions()
                .GetByID(page_url.host());
        if (extension) {
          base::string16 title = base::UTF8ToUTF16(extension->name());
          process.titles.push_back(title);
          process.renderer_type =
              ProcessMemoryInformation::RENDERER_EXTENSION;
          continue;
        }
      }

      extensions::ViewType type = extensions::GetViewType(contents);
      if (type == extensions::VIEW_TYPE_BACKGROUND_CONTENTS) {
        process.titles.push_back(base::UTF8ToUTF16(page_url.spec()));
        process.renderer_type =
            ProcessMemoryInformation::RENDERER_BACKGROUND_APP;
        continue;
      }
#endif

      base::string16 title = contents->GetTitle();
      if (!title.length())
        title = l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
      process.titles.push_back(title);
    }

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
    if (service_manager::ZygoteHost::GetInstance()->IsZygotePid(process.pid)) {
      process.process_type = content::PROCESS_TYPE_ZYGOTE;
    }
#endif
  }

  // Get rid of other Chrome processes that are from a different profile.
  auto is_unknown = [](ProcessMemoryInformation& process) {
    return process.process_type == content::PROCESS_TYPE_UNKNOWN;
  };
  auto& vector = chrome_browser->processes;
  base::EraseIf(vector, is_unknown);

  // Grab a memory dump for all processes.
  // Using AdaptCallbackForRepeating allows for an easier transition to
  // OnceCallbacks for https://crbug.com/714018.
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestPrivateMemoryFootprint(
          base::kNullProcessId,
          base::AdaptCallbackForRepeating(
              base::BindOnce(&MemoryDetails::DidReceiveMemoryDump, this)));
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
