// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/web_contents_task_provider.h"

#include <memory>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "chrome/browser/task_manager/providers/web_contents/back_forward_cache_task.h"
#include "chrome/browser/task_manager/providers/web_contents/fenced_frame_task.h"
#include "chrome/browser/task_manager/providers/web_contents/prerender_task.h"
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
  void RenderFrameHostStateChanged(
      RenderFrameHost* render_frame_host,
      RenderFrameHost::LifecycleState old_state,
      RenderFrameHost::LifecycleState new_state) override;
  void WebContentsDestroyed() override;
  void OnRendererUnresponsive(RenderProcessHost* render_process_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void TitleWasSet(content::NavigationEntry* entry) override;

  void RenderFrameReady(int process_routing_id, int frame_routing_id);

 private:
  // Defines a callback for WebContents::ForEachRenderFrameHost() to create a
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
  raw_ptr<WebContentsTaskProvider> provider_;

  // For each SiteInstance, the frames within it that are tracked (local roots
  // only as per FindLocalRoot()), and its RendererTask. The number of tracked
  // items is small, thus flat_map and flat_set.
  struct SiteInstanceInfo {
    base::flat_set<raw_ptr<RenderFrameHost, CtnExperimental>> frames;
    std::unique_ptr<RendererTask> renderer_task;
  };
  base::flat_map<SiteInstance*, SiteInstanceInfo> site_instance_infos_;

  // States whether we did record a main frame for this entry.
  raw_ptr<SiteInstance> primary_main_frame_site_instance_ = nullptr;

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
  DCHECK(web_contents()->GetPrimaryMainFrame());
  web_contents()->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* render_frame_host) {
        const auto state = render_frame_host->GetLifecycleState();
        // `WebContents::ForEachRenderFrameHost` does not iterate over
        // speculative or pending commit RFHs.
        //
        // TODO(crbug.com/40262518): Move this CHECK into
        // `WebContents::ForEachRenderFrameHost`.
        CHECK_NE(state, RenderFrameHost::LifecycleState::kPendingCommit);
        // TODO(crbug.com/40262518):
        // `WebContents::ForEachRenderFrameHost` should explicitly exclude
        // `kPendingDeletion`, just like `kSpeculative` and `kPendingCommit`.
        if (state == RenderFrameHost::LifecycleState::kPendingDeletion) {
          // A `kPendingDeletion` RFH will soon be destroyed. The task manager
          // does not need to create a task for such a RFH.
          return;
        }
        CreateTaskForFrame(render_frame_host);
      });
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
  primary_main_frame_site_instance_ = nullptr;
}

RendererTask* WebContentsTaskProvider::WebContentsEntry::GetTaskForFrame(
    RenderFrameHost* render_frame_host) const {
  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  auto itr = site_instance_infos_.find(site_instance);
  if (itr == site_instance_infos_.end())
    return nullptr;

  // Only local roots are in the frame list.
  if (!itr->second.frames.count(FindLocalRoot(render_frame_host)))
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
  // The navigating frame and its subframes are now pending deletion. Stop
  // tracking them immediately rather than when they are destroyed. The order of
  // deletion is important. The children must be removed first.
  if (old_host) {
    ClearTasksForDescendantsOf(old_host);
    ClearTaskForFrame(old_host);
  }
  // Tasks creation for |new_host| is delayed to |DidFinishNavigation|.
}

// Handles creation and deletion of BFCache tasks for pages entering and leaving
// the BFCache, and deletion of prerender tasks after prerendering activation.
void WebContentsTaskProvider::WebContentsEntry::RenderFrameHostStateChanged(
    RenderFrameHost* render_frame_host,
    RenderFrameHost::LifecycleState old_state,
    RenderFrameHost::LifecycleState new_state) {
  if (old_state == RenderFrameHost::LifecycleState::kInBackForwardCache)
    ClearTaskForFrame(render_frame_host);

  if (new_state == RenderFrameHost::LifecycleState::kInBackForwardCache)
    CreateTaskForFrame(render_frame_host);

  // |RenderFrameDeleted| is not fired for prerender page activation so we need
  // to delete prerender task. |RenderFrameHostStateChanged| is the earliest
  // event of the prerendering activation flow.
  if (old_state == RenderFrameHost::LifecycleState::kPrerendering &&
      new_state == RenderFrameHost::LifecycleState::kActive) {
    ClearTaskForFrame(render_frame_host);
  }
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
  // No task creation/update for downloads and HTTP204/205.
  if (!navigation_handle->HasCommitted()) {
    return;
  }

  // Creation.
  //
  // Create a task if we have not encountered this site instance before, or it
  // is a new RenderFrameHost for this site instance. For same-document
  // navigation, since neither |RenderFrameDeleted| nor |RenderFrameHostChanged|
  // is fired to delete the existing task, we do not recreate them.
  //
  // TODO(crbug.com/40171294): DidFinishNavigation is not called when we
  // create initial empty documents, and as a result, we will not create new
  // tasks for these empty documents if they are in a different process from
  // their embedder/opener (eg: an empty fenced frame or a blank tab created by
  // window.open('', '_blank', 'noopener')). Ideally, we would call
  // CreateTaskForFrame inside RenderFrameCreated instead (which is called for
  // initial documents), but CreateTaskForFrame uses RFH::GetLifecycleState,
  // which cannot currently be called inside RenderFrameCreated (due to a DCHECK
  // which doesn't allow the method to be called when the state is
  // 'kSpeculative').
  {
    auto* rfh = navigation_handle->GetRenderFrameHost();
    auto* site_instance = rfh->GetSiteInstance();
    auto it = site_instance_infos_.find(site_instance);
    if (!navigation_handle->IsSameDocument() &&
        (it == site_instance_infos_.end() ||
         it->second.frames.find(rfh) == it->second.frames.end())) {
      CreateTaskForFrame(rfh);
    }
  }

  // Update.
  //
  // We only need to update tasks for primary main frame navigations.
  // FencedFrame task gets the title from |SiteInstance::GetSiteURL()| which
  // does not change for the same site instance, thus no need to update;
  // prerender does not support multiple navigations thus no need to update its
  // title.
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  RendererTask* main_frame_task =
      GetTaskForFrame(web_contents()->GetPrimaryMainFrame());
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
  DCHECK(render_frame_host);
  // Currently we do not track speculative RenderFrameHosts or RenderFrameHosts
  // which are pending deletion.
  const auto rfh_state = render_frame_host->GetLifecycleState();
  switch (rfh_state) {
    case RenderFrameHost::LifecycleState::kPrerendering:
    case RenderFrameHost::LifecycleState::kInBackForwardCache:
    case RenderFrameHost::LifecycleState::kActive:
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Illegal RFH state for TaskManager: "
                                << static_cast<int>(rfh_state);
      break;
  }

  // Exclude sad tabs, sad OOPIFs.
  if (!render_frame_host->IsRenderFrameLive()) {
    return;
  }

  // Another instance of this class will be created for inner WebContents. If we
  // iterate into an inner WebContents that is not associated with `this`, skip
  // it, so we don't create duplicated tasks. Task creation for RenderFrameHosts
  // not associated with a WebContents should be handled by a different type of
  // TaskProvider.
  if (content::WebContents::FromRenderFrameHost(render_frame_host) !=
      web_contents()) {
    return;
  }

  // Exclude frames in the same SiteInstance or same process as their parent;
  // |site_instance_infos_| only contains local roots.
  content::SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  auto* parent = render_frame_host->GetParentOrOuterDocument();
  if (parent && (site_instance == parent->GetSiteInstance() ||
                 site_instance->GetProcess() ==
                     parent->GetSiteInstance()->GetProcess())) {
    return;
  }

  bool site_instance_exists = site_instance_infos_.count(site_instance) != 0;
  bool is_primary_main_frame = render_frame_host->IsInPrimaryMainFrame();
  bool site_instance_is_main =
      (site_instance == primary_main_frame_site_instance_);

  std::unique_ptr<RendererTask> new_task;

  // We need to create a task if one doesn't already exist for this
  // SiteInstance, or if the main frame navigates to a process that currently is
  // represented by a SubframeTask.
  if (!site_instance_exists ||
      (is_primary_main_frame && !site_instance_is_main)) {
    auto* primary_main_frame_task =
        GetTaskForFrame(web_contents()->GetPrimaryMainFrame());
    if (rfh_state == RenderFrameHost::LifecycleState::kInBackForwardCache) {
      // Use RFH::GetMainFrame instead web_contents()->GetPrimaryMainFrame()
      // because the BFCached frames are not the currently active main frame.
      RenderFrameHost* main_frame = render_frame_host->GetMainFrame();
      new_task = std::make_unique<BackForwardCacheTask>(
          render_frame_host, GetTaskForFrame(main_frame), provider_);
    } else if (rfh_state == RenderFrameHost::LifecycleState::kPrerendering) {
      new_task = std::make_unique<PrerenderTask>(render_frame_host, provider_);
    } else if (is_primary_main_frame) {
      const WebContentsTag* tag =
          WebContentsTag::FromWebContents(web_contents());
      new_task = tag->CreateTask(provider_);
      primary_main_frame_site_instance_ = site_instance;
    } else if (render_frame_host->IsFencedFrameRoot()) {
      new_task = std::make_unique<FencedFrameTask>(render_frame_host,
                                                   primary_main_frame_task);
    } else {
      new_task = std::make_unique<SubframeTask>(render_frame_host,
                                                primary_main_frame_task);
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

    if (site_instance == primary_main_frame_site_instance_)
      primary_main_frame_site_instance_ = nullptr;
  }

#if DCHECK_IS_ON()
  // Whenever we have a task, we should have a main frame site instance.
  // However, when a tab is destroyed and there was a BFCached Task or a
  // prerender task, the main task may be cleaned up before the
  // BFCached/prerender Task. BFCache or prerender tasks will be deleted
  // asynchronously after the main frame is deleted.

  bool only_bfcache_or_prerender_rfhs = true;
  for (auto& [ignore, site_instance_info] : site_instance_infos_) {
    for (RenderFrameHost* rfh : site_instance_info.frames) {
      const auto state = rfh->GetLifecycleState();
      if (state != RenderFrameHost::LifecycleState::kInBackForwardCache &&
          state != RenderFrameHost::LifecycleState::kPrerendering) {
        only_bfcache_or_prerender_rfhs = false;
      }
    }
  }
  DCHECK(only_bfcache_or_prerender_rfhs ||
         site_instance_infos_.empty() ==
             (primary_main_frame_site_instance_ == nullptr));
#endif
}

void WebContentsTaskProvider::WebContentsEntry::ClearTasksForDescendantsOf(
    RenderFrameHost* ancestor) {
  ancestor->ForEachRenderFrameHost(
      [this, ancestor](content::RenderFrameHost* render_frame_host) {
        if (render_frame_host == ancestor)
          return;
        ClearTaskForFrame(render_frame_host);
      });
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
  CHECK(itr != entries_map_.end(), base::NotFatalUntil::M130);

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
  for (const task_manager::WebContentsTag* tag : tags_manager->tracked_tags()) {
    OnWebContentsTagCreated(tag);
  }

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
