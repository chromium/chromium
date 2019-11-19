// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/renderer_uptime_tracker.h"

#include "base/metrics/histogram_macros.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/process_map.h"
#endif

namespace metrics {

namespace {

RendererUptimeTracker* g_renderer_uptime_tracker_instance = nullptr;

}  // namespace

// static
void RendererUptimeTracker::Initialize() {
  DCHECK(!g_renderer_uptime_tracker_instance);
  g_renderer_uptime_tracker_instance = new RendererUptimeTracker;
}

// static
RendererUptimeTracker* RendererUptimeTracker::SetMockRendererUptimeTracker(
    RendererUptimeTracker* tracker) {
  RendererUptimeTracker* old_tracker = g_renderer_uptime_tracker_instance;
  g_renderer_uptime_tracker_instance = tracker;
  return old_tracker;
}

// static
RendererUptimeTracker* RendererUptimeTracker::Get() {
  // This can return null in unit tests.
  return g_renderer_uptime_tracker_instance;
}

RendererUptimeTracker::RendererUptimeTracker() {
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

RendererUptimeTracker::~RendererUptimeTracker() {}

void RendererUptimeTracker::OnRendererStarted(int pid) {
  info_map_[pid] = RendererInfo{base::TimeTicks::Now(), 0};
}

base::TimeDelta RendererUptimeTracker::GetProcessUptime(int pid) {
  auto it = info_map_.find(pid);
  // The pid may not exist when process fails to start up or when process is
  // terminated without reuse.
  if (it == info_map_.end())
    return base::TimeDelta();
  return base::TimeTicks::Now() - it->second.launched_at_;
}

void RendererUptimeTracker::OnRendererTerminated(int pid) {
  auto it = info_map_.find(pid);
  // The pid may not exist when process fails to start up or when process is
  // terminated without reuse.
  if (it != info_map_.end()) {
    auto uptime = base::TimeTicks::Now() - it->second.launched_at_;
    UMA_HISTOGRAM_CUSTOM_TIMES("Memory.Experimental.Renderer.Uptime", uptime,
                               base::TimeDelta::FromHours(1),
                               base::TimeDelta::FromDays(7), 50);
    UMA_HISTOGRAM_COUNTS_10000(
        "Memory.Experimental.Renderer.LoadsInMainFrameDuringUptime",
        it->second.num_loads_in_main_frame_);
    info_map_.erase(it);
  }
}

void RendererUptimeTracker::OnLoadInMainFrame(int pid) {
  auto it = info_map_.find(pid);
  // The pid may not exist in an in-process browser test.
  if (it != info_map_.end()) {
    it->second.num_loads_in_main_frame_++;
  }
}

void RendererUptimeTracker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      content::RenderProcessHost* host =
          content::Source<content::RenderProcessHost>(source).ptr();
#if BUILDFLAG(ENABLE_EXTENSIONS)
      if (extensions::ProcessMap::Get(host->GetBrowserContext())
              ->Contains(host->GetID())) {
        break;
      }
#endif
      OnRendererStarted(host->GetID());
      break;
    }

    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      content::RenderProcessHost* host =
          content::Source<content::RenderProcessHost>(source).ptr();
      OnRendererTerminated(host->GetID());
      break;
    }

    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      content::RenderProcessHost* host =
          content::Source<content::RenderProcessHost>(source).ptr();
      OnRendererTerminated(host->GetID());
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

}  // namespace metrics
