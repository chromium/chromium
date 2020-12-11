// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/web_contents_task_provider.h"

#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/task_manager/providers/web_contents/subframe_task.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tags_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

using content::RenderFrameHost;
using content::RenderProcessHost;
using content::SiteInstance;
using content::WebContents;

namespace task_manager {

// Defines an entry for each WebContents that will be tracked by the provider.
// The entry is used to observe certain events in its corresponding WebContents
// and then it notifies the provider or the render task (representing the
// WebContents) of these events.
// The entry owns the created tasks representing the WebContents, and it is
// itself owned by the provider.
class WebContentsTaskProvider::WebContentsEntry
    : public content::WebContentsObserver {
 public:
  WebContentsEntry(content::WebContents* web_contents,
                   WebContentsTaskProvider* provider);
  ~WebContentsEntry() override;
  WebContentsEntry(const WebContentsEntry&) = delete;
  WebContentsEntry& operator=(const WebContentsEntry&) = delete;

  // Creates all the tasks associated with each |RenderFrameHost| in this
  // entry's WebContents.
  void CreateAllTasks();

  // Clears all the tasks in this entry. The provider's observer will be
  // notified if |notify_observer| is true.
  void ClearAllTasks(bool notify_observer);

  // Returns the |RendererTask| that corresponds to the given
  // |render_frame_host| or |nullptr| if the given frame is not tracked by this
  // entry.
  RendererTask* GetTaskForFrame(RenderFrameHost* render_frame_host) const;

  // content::WebContentsObserver:
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void RenderFrameCreated(RenderFrameHost*) override;
  void WebContentsDestroyed() override;
  void OnRendererUnresponsive(RenderProcessHost* render_process_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void TitleWasSet(content::NavigationEntry* entry) override;

  void RenderFrameReady(int process_routing_id, int frame_routing_id);

 private:
  // Defines a callback for WebContents::ForEachFrame() to create a
  // corresponding task for the given |render_frame_host| and notifying the
  // provider's observer of the new task.
  void CreateTaskForFrame(RenderFrameHost* render_frame_host);

  // Clears the task that corresponds to the given |render_frame_host| and
  // notifies the provider's observer of the tasks removal.
  void ClearTaskForFrame(RenderFrameHost* render_frame_host);

  // Same as |ClearTaskForFrame|, but for every descendant of
  // |ancestor|.
  void ClearTasksForDescendantsOf(RenderFrameHost* ancestor);

  // Walks parents until hitting a process boundary. Returns the highest frame
  // in the same SiteInstance as |render_frame_host|.
  RenderFrameHost* FindLocalRoot(RenderFrameHost* render_frame_host) const;

  // The provider that owns this entry.
  WebContentsTaskProvider* provider_;

  // For each SiteInstance, the frames within it that are tracked (local roots
  // only as per FindLocalRoot()), and its RendererTask. The number of tracked
  // items is small, thus flat_map and flat_set.
  struct SiteInstanceInfo {
    base::flat_set<RenderFrameHost*> frames;
    std::unique_ptr<RendererTask> renderer_task;
  };
  base::flat_map<SiteInstance*, SiteInstanceInfo> site_instance_infos_;

  // States whether we did record a main frame for this entry.
  SiteInstance* main_frame_site_instance_ = nullptr;

  base::WeakPtrFactory<WebContentsEntry> weak_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////

WebContentsTaskProvider::WebContentsEntry::WebContentsEntry(
    content::WebContents* web_contents,
    WebContentsTaskProvider* provider)
    : WebContentsObserver(web_contents), provider_(provider) {}

WebContentsTaskProvider::WebContentsEntry::~WebContentsEntry() {
  ClearAllTasks(false);
}

void WebContentsTaskProvider::WebContentsEntry::CreateAllTasks() {
  DCHECK(web_contents()->GetMainFrame());
  web_contents()->ForEachFrame(base::BindRepeating(
      &WebContentsEntry::CreateTaskForFrame, base::Unretained(this)));
}

void WebContentsTaskProvider::WebContentsEntry::ClearAllTasks(
    bool notify_observer) {
  for (auto& it : site_instance_infos_) {
    RendererTask* task = it.second.renderer_task.get();
    task->set_termination_status(web_contents()->GetCrashedStatus());
    task->set_termination_error_code(web_contents()->GetCrashedErrorCode());

    if (notify_observer)
      provider_->NotifyObserverTaskRemoved(task);
  }

  site_instance_infos_.clear();
  main_frame_site_instance_ = nullptr;
}

RendererTask* WebContentsTaskProvider::WebContentsEntry::GetTaskForFrame(
    RenderFrameHost* render_frame_host) const {
  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  auto itr = site_instance_infos_.find(site_instance);
  if (itr == site_instance_infos_.end())
    return nullptr;

  // Only local roots are in the frame list.
  if (!itr->second.frames.count(render_frame_host))
    return nullptr;

  return itr->second.renderer_task.get();
}

RenderFrameHost* WebContentsTaskProvider::WebContentsEntry::FindLocalRoot(
    RenderFrameHost* render_frame_host) const {
  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  RenderFrameHost* candidate = render_frame_host;
  while (RenderFrameHost* parent = candidate->GetParent()) {
    if (parent->GetSiteInstance() != site_instance)
      break;
    candidate = parent;
  }
  return candidate;
}

void WebContentsTaskProvider::WebContentsEntry::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  ClearTaskForFrame(render_frame_host);
}

void WebContentsTaskProvider::WebContentsEntry::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  DCHECK(new_host->IsCurrent());

  // The navigating frame and its subframes are now pending deletion. Stop
  // tracking them immediately rather than when they are destroyed. The order of
  // deletion is important. The children must be removed first.
  if (old_host) {
    ClearTasksForDescendantsOf(old_host);
    ClearTaskForFrame(old_host);
  }

  CreateTaskForFrame(new_host);
}

void WebContentsTaskProvider::WebContentsEntry::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host->IsRenderFrameLive());

  // Skip pending/speculative hosts. We'll create tasks for these if the
  // navigation commits, at which point RenderFrameHostChanged() will fire.
  if (!render_frame_host->IsCurrent())
    return;

  CreateTaskForFrame(render_frame_host);
}

void WebContentsTaskProvider::WebContentsEntry::RenderFrameReady(
    int render_process_id,
    int render_frame_id) {
  // We get here when a RenderProcessHost we are tracking transitions to the
  // IsReady state. This might mean we know its process ID.
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return;

  Task* task = GetTaskForFrame(render_frame_host);

  if (!task)
    return;

  const base::ProcessId determine_pid_from_handle = base::kNullProcessId;
  provider_->UpdateTaskProcessInfoAndNotifyObserver(
      task, render_frame_host->GetProcess()->GetProcess().Handle(),
      determine_pid_from_handle);
}

void WebContentsTaskProvider::WebContentsEntry::WebContentsDestroyed() {
  ClearAllTasks(true);
  provider_->DeleteEntry(web_contents());
}

void WebContentsTaskProvider::WebContentsEntry::OnRendererUnresponsive(
    RenderProcessHost* render_process_host) {
  for (const auto& pair : site_instance_infos_) {
    if (pair.first->GetProcess() == render_process_host) {
      provider_->NotifyObserverTaskUnresponsive(
          pair.second.renderer_task.get());
      return;
    }
  }
}

void WebContentsTaskProvider::WebContentsEntry::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // We only need to update tasks for main frame navigations.
  if (!navigation_handle->IsInMainFrame())
    return;

  RendererTask* main_frame_task =
      GetTaskForFrame(web_contents()->GetMainFrame());
  if (!main_frame_task)
    return;

  for (auto& it : site_instance_infos_) {
    RendererTask* task = it.second.renderer_task.get();

    // Listening to WebContentsObserver::TitleWasSet() only is not enough in
    // some cases when the the web page doesn't have a title. That's why we
    // update the title here as well.
    task->UpdateTitle();

    // Call RendererTask::UpdateFavicon() to set the current favicon to the
    // default favicon. If the page has a non-default favicon,
    // RendererTask::OnFaviconUpdated() will update the current favicon once
    // FaviconDriver figures out the correct favicon for the page.
    task->UpdateFavicon();
  }
}

void WebContentsTaskProvider::WebContentsEntry::TitleWasSet(
    content::NavigationEntry* entry) {
  for (auto& it : site_instance_infos_) {
    RendererTask* task = it.second.renderer_task.get();
    task->UpdateTitle();
    task->UpdateFavicon();
  }
}

void WebContentsTaskProvider::WebContentsEntry::CreateTaskForFrame(
    RenderFrameHost* render_frame_host) {
  // Currently we do not track pending hosts, or pending delete hosts.
  DCHECK(render_frame_host);
  DCHECK(render_frame_host->IsCurrent());

  // Exclude sad tabs and sad OOPIFs.
  if (!render_frame_host->IsRenderFrameLive())
    return;

  // Exclude frames in the same SiteInstance as their parent;
  // |site_instance_by_frames_| only contains local roots.
  content::SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  if (render_frame_host->GetParent() &&
      site_instance == render_frame_host->GetParent()->GetSiteInstance()) {
    return;
  }

  bool site_instance_exists = site_instance_infos_.count(site_instance) != 0;
  bool is_main_frame = (render_frame_host == web_contents()->GetMainFrame());
  bool site_instance_is_main = (site_instance == main_frame_site_instance_);

  std::unique_ptr<RendererTask> new_task;

  // We need to create a task if one doesn't already exist for this
  // SiteInstance, or if the main frame navigates to a process that currently is
  // represented by a SubframeTask.
  if (!site_instance_exists || (is_main_frame && !site_instance_is_main)) {
    if (is_main_frame) {
      const WebContentsTag* tag =
          WebContentsTag::FromWebContents(web_contents());
      new_task = tag->CreateTask(provider_);
      main_frame_site_instance_ = site_instance;
    } else {
      new_task = std::make_unique<SubframeTask>(
          render_frame_host, web_contents(),
          GetTaskForFrame(web_contents()->GetMainFrame()));
    }
  }

  auto insert_result =
      site_instance_infos_[site_instance].frames.insert(render_frame_host);
  DCHECK(insert_result.second);

  if (new_task) {
    if (site_instance_exists) {
      provider_->NotifyObserverTaskRemoved(
          site_instance_infos_[site_instance].renderer_task.get());
    }

    RendererTask* new_task_ptr = new_task.get();
    site_instance_infos_[site_instance].renderer_task = std::move(new_task);
    provider_->NotifyObserverTaskAdded(new_task_ptr);

    // If we don't know the OS process handle yet (e.g., because this task is
    // still launching), update the task when it becomes available.
    if (new_task_ptr->process_id() == base::kNullProcessId) {
      render_frame_host->GetProcess()->PostTaskWhenProcessIsReady(
          base::BindOnce(&WebContentsEntry::RenderFrameReady,
                         weak_factory_.GetWeakPtr(),
                         render_frame_host->GetProcess()->GetID(),
                         render_frame_host->GetRoutingID()));
    }
  }
}

void WebContentsTaskProvider::WebContentsEntry::ClearTaskForFrame(
    RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);
  content::SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  auto itr = site_instance_infos_.find(site_instance);
  if (itr == site_instance_infos_.end())
    return;

  auto& frames = itr->second.frames;
  size_t count = frames.erase(render_frame_host);
  if (!count)
    return;

  if (frames.empty()) {
    std::unique_ptr<RendererTask> renderer_task =
        std::move(itr->second.renderer_task);
    site_instance_infos_.erase(itr);
    provider_->NotifyObserverTaskRemoved(renderer_task.get());

    if (site_instance == main_frame_site_instance_)
      main_frame_site_instance_ = nullptr;
  }

  // Whenever we have a task, we should have a main frame site instance.
  DCHECK(site_instance_infos_.empty() ==
         (main_frame_site_instance_ == nullptr));
}

void WebContentsTaskProvider::WebContentsEntry::ClearTasksForDescendantsOf(
    RenderFrameHost* ancestor) {
  // 1) Collect descendants.
  base::flat_set<RenderFrameHost*> descendants;
  for (const auto& site_instance_pair : site_instance_infos_) {
    for (auto* frame : site_instance_pair.second.frames) {
      if (frame->IsDescendantOf(ancestor))
        descendants.insert(frame);
    }
  }

  // 2) Delete them.
  for (RenderFrameHost* rfh : descendants)
    ClearTaskForFrame(rfh);
}

////////////////////////////////////////////////////////////////////////////////

WebContentsTaskProvider::WebContentsTaskProvider() = default;

WebContentsTaskProvider::~WebContentsTaskProvider() {
  if (is_updating_) {
    StopUpdating();
  }
}

void WebContentsTaskProvider::OnWebContentsTagCreated(
    const WebContentsTag* tag) {
  DCHECK(tag);
  content::WebContents* web_contents = tag->web_contents();
  DCHECK(web_contents);

  // TODO(afakhry): Check if we need this check. It seems that we no longer
  // need it in the new implementation.
  std::unique_ptr<WebContentsEntry>& entry = entries_map_[web_contents];
  if (entry) {
    // This case may happen if we added a WebContents while collecting all the
    // pre-existing ones at the time |StartUpdating()| was called, but the
    // notification of its connection hasn't been fired yet. In this case we
    // ignore it since we're already tracking it.
    return;
  }

  entry = std::make_unique<WebContentsEntry>(web_contents, this);
  entry->CreateAllTasks();
}

void WebContentsTaskProvider::OnWebContentsTagRemoved(
    const WebContentsTag* tag) {
  DCHECK(tag);
  content::WebContents* web_contents = tag->web_contents();
  DCHECK(web_contents);

  auto itr = entries_map_.find(web_contents);
  DCHECK(itr != entries_map_.end());

  // Must manually clear the tasks and notify the observer.
  itr->second->ClearAllTasks(true);
  entries_map_.erase(itr);  // Deletes the WebContentsEntry.
}

Task* WebContentsTaskProvider::GetTaskOfUrlRequest(int child_id, int route_id) {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(child_id, route_id);
  return GetTaskOfFrame(rfh);
}

bool WebContentsTaskProvider::HasWebContents(
    content::WebContents* web_contents) const {
  return entries_map_.count(web_contents) != 0;
}

Task* WebContentsTaskProvider::GetTaskOfFrame(content::RenderFrameHost* rfh) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);

  if (!web_contents)
    return nullptr;

  auto itr = entries_map_.find(web_contents);
  if (itr == entries_map_.end()) {
    // Can happen if the tab was closed while a network request was being
    // performed.
    return nullptr;
  }

  return itr->second->GetTaskForFrame(rfh);
}

void WebContentsTaskProvider::StartUpdating() {
  is_updating_ = true;

  // 1- Collect all pre-existing WebContents from the WebContentsTagsManager.
  WebContentsTagsManager* tags_manager = WebContentsTagsManager::GetInstance();
  for (const auto* tag : tags_manager->tracked_tags())
    OnWebContentsTagCreated(tag);

  // 2- Start observing newly connected ones.
  tags_manager->SetProvider(this);
}

void WebContentsTaskProvider::StopUpdating() {
  is_updating_ = false;

  // 1- Stop observing.
  WebContentsTagsManager::GetInstance()->ClearProvider();

  // 2- Clear storage.
  entries_map_.clear();
}

void WebContentsTaskProvider::DeleteEntry(content::WebContents* web_contents) {
  // This erase() will delete the WebContentsEntry, which is actually our
  // caller, but it's expecting us to delete it.
  bool success = entries_map_.erase(web_contents) != 0;
  DCHECK(success);
}

}  // namespace task_manager
