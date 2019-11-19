// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_link_manager.h"

#include <functional>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/common/prerender.mojom.h"
#include "chrome/common/prerender_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/common/referrer.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/prerender/prerender_rel_type.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "components/guest_view/browser/guest_view_base.h"
#endif

using base::TimeDelta;
using base::TimeTicks;
using content::RenderViewHost;
using content::SessionStorageNamespace;

namespace prerender {

namespace {

constexpr int kRelTypeHistogramEnumMax =
    (blink::kPrerenderRelTypePrerender | blink::kPrerenderRelTypeNext) + 1;

void RecordLinkManagerAdded(const uint32_t rel_types) {
  UMA_HISTOGRAM_ENUMERATION("Prerender.RelTypesLinkAdded",
                            rel_types & (kRelTypeHistogramEnumMax - 1),
                            kRelTypeHistogramEnumMax);
}

void RecordLinkManagerStarting(const uint32_t rel_types) {
  UMA_HISTOGRAM_ENUMERATION("Prerender.RelTypesLinkStarted",
                            rel_types & (kRelTypeHistogramEnumMax - 1),
                            kRelTypeHistogramEnumMax);
}

mojo::AssociatedRemote<chrome::mojom::PrerenderDispatcher>
GetPrerenderDispatcher(int child_id) {
  mojo::AssociatedRemote<chrome::mojom::PrerenderDispatcher>
      prerender_dispatcher;
  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(child_id);
  if (render_process_host) {
    IPC::ChannelProxy* channel = render_process_host->GetChannel();
    // |channel| might be NULL in tests.
    if (channel) {
      channel->GetRemoteAssociatedInterface(&prerender_dispatcher);
    }
  }
  return prerender_dispatcher;
}

}  // namespace

// Helper class to implement PrerenderContents::Observer and watch prerenders
// which launch other prerenders.
class PrerenderLinkManager::PendingPrerenderManager
    : public PrerenderContents::Observer {
 public:
  explicit PendingPrerenderManager(PrerenderLinkManager* link_manager)
      : link_manager_(link_manager) {}

  ~PendingPrerenderManager() override { CHECK(observed_launchers_.empty()); }

  void ObserveLauncher(PrerenderContents* launcher) {
    DCHECK_EQ(FINAL_STATUS_UNKNOWN, launcher->final_status());
    bool inserted = observed_launchers_.insert(launcher).second;
    if (inserted)
      launcher->AddObserver(this);
  }

  void OnPrerenderStart(PrerenderContents* launcher) override {}

  void OnPrerenderStop(PrerenderContents* launcher) override {
    observed_launchers_.erase(launcher);
    if (launcher->final_status() == FINAL_STATUS_USED) {
      link_manager_->StartPendingPrerendersForLauncher(launcher);
    } else {
      link_manager_->CancelPendingPrerendersForLauncher(launcher);
    }
  }

  void OnPrerenderNetworkBytesChanged(PrerenderContents* launcher) override {}

 private:
  // A pointer to the parent PrerenderLinkManager.
  PrerenderLinkManager* link_manager_;

  // The set of PrerenderContentses being observed. Lifetimes are managed by
  // OnPrerenderStop.
  std::set<PrerenderContents*> observed_launchers_;
};

PrerenderLinkManager::PrerenderLinkManager(PrerenderManager* manager)
    : has_shutdown_(false),
      manager_(manager),
      pending_prerender_manager_(
          std::make_unique<PendingPrerenderManager>(this)) {}

PrerenderLinkManager::~PrerenderLinkManager() {
  for (auto& prerender : prerenders_) {
    if (prerender.handle) {
      DCHECK(!prerender.handle->IsPrerendering())
          << "All running prerenders should stop at the same time as the "
          << "PrerenderManager.";
      delete prerender.handle;
      prerender.handle = nullptr;
    }
  }
}

void PrerenderLinkManager::OnAddPrerender(int launcher_child_id,
                                          int prerender_id,
                                          const GURL& url,
                                          uint32_t rel_types,
                                          const content::Referrer& referrer,
                                          const url::Origin& initiator_origin,
                                          const gfx::Size& size,
                                          int render_view_route_id) {
  DCHECK_EQ(nullptr, FindByLauncherChildIdAndPrerenderId(launcher_child_id,
                                                         prerender_id));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(launcher_child_id, render_view_route_id);
  content::WebContents* web_contents =
      rvh ? content::WebContents::FromRenderViewHost(rvh) : nullptr;
  // Guests inside <webview> do not support cross-process navigation and so we
  // do not allow guests to prerender content.
  if (guest_view::GuestViewBase::IsGuest(web_contents))
    return;
#endif

  // Check if the launcher is itself an unswapped prerender.
  PrerenderContents* prerender_contents =
      manager_->GetPrerenderContentsForRoute(launcher_child_id,
                                             render_view_route_id);
  if (prerender_contents &&
      prerender_contents->final_status() != FINAL_STATUS_UNKNOWN) {
    // The launcher is a prerender about to be destroyed asynchronously, but
    // its AddLinkRelPrerender message raced with shutdown. Ignore it.
    DCHECK_NE(FINAL_STATUS_USED, prerender_contents->final_status());
    return;
  }

  LinkPrerender prerender(launcher_child_id, prerender_id, url, rel_types,
                          referrer, initiator_origin, size,
                          render_view_route_id, manager_->GetCurrentTimeTicks(),
                          prerender_contents);
  prerenders_.push_back(prerender);
  RecordLinkManagerAdded(rel_types);
  if (prerender_contents)
    pending_prerender_manager_->ObserveLauncher(prerender_contents);
  else
    StartPrerenders();
}

void PrerenderLinkManager::OnCancelPrerender(int child_id, int prerender_id) {
  LinkPrerender* prerender = FindByLauncherChildIdAndPrerenderId(child_id,
                                                                 prerender_id);
  if (!prerender)
    return;

  CancelPrerender(prerender);
  StartPrerenders();
}

void PrerenderLinkManager::OnAbandonPrerender(int child_id, int prerender_id) {
  LinkPrerender* prerender = FindByLauncherChildIdAndPrerenderId(child_id,
                                                                 prerender_id);
  if (!prerender)
    return;

  if (!prerender->handle) {
    RemovePrerender(prerender);
    return;
  }

  prerender->has_been_abandoned = true;
  prerender->handle->OnNavigateAway();
  DCHECK(prerender->handle);

  // If the prerender is not running, remove it from the list so it does not
  // leak. If it is running, it will send a cancel event when it stops which
  // will remove it.
  if (!prerender->handle->IsPrerendering())
    RemovePrerender(prerender);
}

void PrerenderLinkManager::OnChannelClosing(int child_id) {
  auto next = prerenders_.begin();
  while (next != prerenders_.end()) {
    auto it = next;
    ++next;

    if (child_id != it->launcher_child_id)
      continue;

    const size_t running_prerender_count = CountRunningPrerenders();
    OnAbandonPrerender(child_id, it->prerender_id);
    DCHECK_EQ(running_prerender_count, CountRunningPrerenders());
  }
}

PrerenderLinkManager::LinkPrerender::LinkPrerender(
    int launcher_child_id,
    int prerender_id,
    const GURL& url,
    uint32_t rel_types,
    const content::Referrer& referrer,
    const url::Origin& initiator_origin,
    const gfx::Size& size,
    int render_view_route_id,
    TimeTicks creation_time,
    PrerenderContents* deferred_launcher)
    : launcher_child_id(launcher_child_id),
      prerender_id(prerender_id),
      url(url),
      rel_types(rel_types),
      referrer(referrer),
      initiator_origin(initiator_origin),
      size(size),
      render_view_route_id(render_view_route_id),
      creation_time(creation_time),
      deferred_launcher(deferred_launcher),
      handle(nullptr),
      has_been_abandoned(false) {}

PrerenderLinkManager::LinkPrerender::LinkPrerender(const LinkPrerender& other) =
    default;

PrerenderLinkManager::LinkPrerender::~LinkPrerender() {
  DCHECK_EQ(nullptr, handle)
      << "The PrerenderHandle should be destroyed before its Prerender.";
}

bool PrerenderLinkManager::IsEmpty() const {
  return prerenders_.empty();
}

size_t PrerenderLinkManager::CountRunningPrerenders() const {
  return std::count_if(prerenders_.begin(), prerenders_.end(),
                       [](const LinkPrerender& prerender) {
                         return prerender.handle &&
                                prerender.handle->IsPrerendering();
                       });
}

void PrerenderLinkManager::StartPrerenders() {
  if (has_shutdown_)
    return;

  size_t total_started_prerender_count = 0;
  std::list<LinkPrerender*> abandoned_prerenders;
  std::list<std::list<LinkPrerender>::iterator> pending_prerenders;
  std::multiset<std::pair<int, int> >
      running_launcher_and_render_view_routes;

  // Scan the list, counting how many prerenders have handles (and so were added
  // to the PrerenderManager). The count is done for the system as a whole, and
  // also per launcher.
  for (auto i = prerenders_.begin(); i != prerenders_.end(); ++i) {
    LinkPrerender& prerender = *i;
    // Skip prerenders launched by a prerender.
    if (prerender.deferred_launcher)
      continue;
    if (!prerender.handle) {
      pending_prerenders.push_back(i);
    } else {
      ++total_started_prerender_count;
      if (prerender.has_been_abandoned) {
        abandoned_prerenders.push_back(&prerender);
      } else {
        // We do not count abandoned prerenders towards their launcher, since it
        // has already navigated on to another page.
        std::pair<int, int> launcher_and_render_view_route(
            prerender.launcher_child_id, prerender.render_view_route_id);
        running_launcher_and_render_view_routes.insert(
            launcher_and_render_view_route);
        DCHECK_GE(manager_->config().max_link_concurrency_per_launcher,
                  running_launcher_and_render_view_routes.count(
                      launcher_and_render_view_route));
      }
    }

    DCHECK_EQ(&prerender,
              FindByLauncherChildIdAndPrerenderId(prerender.launcher_child_id,
                                                  prerender.prerender_id));
  }
  DCHECK_LE(abandoned_prerenders.size(), total_started_prerender_count);
  DCHECK_GE(manager_->config().max_link_concurrency,
            total_started_prerender_count);
  DCHECK_LE(CountRunningPrerenders(), total_started_prerender_count);

  TimeTicks now = manager_->GetCurrentTimeTicks();

  // Scan the pending prerenders, starting prerenders as we can.
  for (std::list<std::list<LinkPrerender>::iterator>::const_iterator
           i = pending_prerenders.begin(), end = pending_prerenders.end();
       i != end; ++i) {
    const std::list<LinkPrerender>::iterator& it = *i;
    TimeDelta prerender_age = now - it->creation_time;
    if (prerender_age >= manager_->config().max_wait_to_launch) {
      // This prerender waited too long in the queue before launching.
      prerenders_.erase(it);
      continue;
    }

    std::pair<int, int> launcher_and_render_view_route(
        it->launcher_child_id, it->render_view_route_id);
    if (manager_->config().max_link_concurrency_per_launcher <=
        running_launcher_and_render_view_routes.count(
            launcher_and_render_view_route)) {
      // This prerender's launcher is already at its limit.
      continue;
    }

    if (total_started_prerender_count >=
            manager_->config().max_link_concurrency ||
        total_started_prerender_count >= prerenders_.size()) {
      // The system is already at its prerender concurrency limit. Try removing
      // an abandoned prerender, if one exists, to make room.
      if (abandoned_prerenders.empty())
        return;

      CancelPrerender(abandoned_prerenders.front());
      --total_started_prerender_count;
      abandoned_prerenders.pop_front();
    }

    if (!(blink::kPrerenderRelTypePrerender & it->rel_types)) {
      prerenders_.erase(it);
      continue;
    }

    std::unique_ptr<PrerenderHandle> handle =
        manager_->AddPrerenderFromLinkRelPrerender(
            it->launcher_child_id, it->render_view_route_id, it->url,
            it->rel_types, it->referrer, it->initiator_origin, it->size);
    if (!handle) {
      // This prerender couldn't be launched, it's gone.
      prerenders_.erase(it);
      continue;
    }

    if (handle->IsPrerendering()) {
      // We have successfully started a new prerender.
      it->handle = handle.release();
      ++total_started_prerender_count;
      it->handle->SetObserver(this);
      OnPrerenderStart(it->handle);
      RecordLinkManagerStarting(it->rel_types);
      running_launcher_and_render_view_routes.insert(
          launcher_and_render_view_route);
    } else {
      content::RenderProcessHost* render_process_host =
          content::RenderProcessHost::FromID(it->launcher_child_id);
      if (!render_process_host)
        return;

      IPC::ChannelProxy* channel = render_process_host->GetChannel();
      // |channel| might be NULL in tests.
      if (channel) {
        mojo::AssociatedRemote<chrome::mojom::PrerenderDispatcher>
            prerender_dispatcher;
        channel->GetRemoteAssociatedInterface(&prerender_dispatcher);
        prerender_dispatcher->PrerenderStop(it->prerender_id);
      }
      prerenders_.erase(it);
    }
  }
}

PrerenderLinkManager::LinkPrerender*
PrerenderLinkManager::FindByLauncherChildIdAndPrerenderId(int launcher_child_id,
                                                          int prerender_id) {
  for (auto& prerender : prerenders_) {
    if (prerender.launcher_child_id == launcher_child_id &&
        prerender.prerender_id == prerender_id) {
      return &prerender;
    }
  }
  return nullptr;
}

PrerenderLinkManager::LinkPrerender*
PrerenderLinkManager::FindByPrerenderHandle(PrerenderHandle* prerender_handle) {
  DCHECK(prerender_handle);
  for (auto& prerender : prerenders_) {
    if (prerender.handle == prerender_handle)
      return &prerender;
  }
  return nullptr;
}

void PrerenderLinkManager::RemovePrerender(LinkPrerender* prerender) {
  for (auto i = prerenders_.begin(); i != prerenders_.end(); ++i) {
    LinkPrerender& current_prerender = *i;
    if (&current_prerender == prerender) {
      std::unique_ptr<PrerenderHandle> own_handle(prerender->handle);
      prerender->handle = nullptr;
      prerenders_.erase(i);
      return;
    }
  }
  NOTREACHED();
}

void PrerenderLinkManager::CancelPrerender(LinkPrerender* prerender) {
  for (auto i = prerenders_.begin(); i != prerenders_.end(); ++i) {
    LinkPrerender& current_prerender = *i;
    if (&current_prerender == prerender) {
      std::unique_ptr<PrerenderHandle> own_handle(prerender->handle);
      prerender->handle = nullptr;
      prerenders_.erase(i);
      if (own_handle)
        own_handle->OnCancel();
      return;
    }
  }
  NOTREACHED();
}

void PrerenderLinkManager::StartPendingPrerendersForLauncher(
    PrerenderContents* launcher) {
  for (auto& prerender : prerenders_) {
    if (prerender.deferred_launcher == launcher)
      prerender.deferred_launcher = nullptr;
  }
  StartPrerenders();
}

void PrerenderLinkManager::CancelPendingPrerendersForLauncher(
    PrerenderContents* launcher) {
  // Remove all pending prerenders for this launcher.
  for (auto i = prerenders_.begin(); i != prerenders_.end();) {
    if (i->deferred_launcher == launcher) {
      DCHECK(!i->handle);
      i = prerenders_.erase(i);
    } else {
      ++i;
    }
  }
}

void PrerenderLinkManager::Shutdown() {
  has_shutdown_ = true;
}

// In practice, this is always called from PrerenderLinkManager::OnAddPrerender.
void PrerenderLinkManager::OnPrerenderStart(
    PrerenderHandle* prerender_handle) {
  LinkPrerender* prerender = FindByPrerenderHandle(prerender_handle);
  if (!prerender)
    return;

  mojo::AssociatedRemote<chrome::mojom::PrerenderDispatcher>
      prerender_dispatcher =
          GetPrerenderDispatcher(prerender->launcher_child_id);
  if (prerender_dispatcher)
    prerender_dispatcher->PrerenderStart(prerender->prerender_id);
}

void PrerenderLinkManager::OnPrerenderStopLoading(
    PrerenderHandle* prerender_handle) {
  LinkPrerender* prerender = FindByPrerenderHandle(prerender_handle);
  if (!prerender)
    return;

  mojo::AssociatedRemote<chrome::mojom::PrerenderDispatcher>
      prerender_dispatcher =
          GetPrerenderDispatcher(prerender->launcher_child_id);
  if (prerender_dispatcher)
    prerender_dispatcher->PrerenderStopLoading(prerender->prerender_id);
}

void PrerenderLinkManager::OnPrerenderDomContentLoaded(
    PrerenderHandle* prerender_handle) {
  LinkPrerender* prerender = FindByPrerenderHandle(prerender_handle);
  if (!prerender)
    return;

  mojo::AssociatedRemote<chrome::mojom::PrerenderDispatcher>
      prerender_dispatcher =
          GetPrerenderDispatcher(prerender->launcher_child_id);
  if (prerender_dispatcher)
    prerender_dispatcher->PrerenderDomContentLoaded(prerender->prerender_id);
}

void PrerenderLinkManager::OnPrerenderStop(
    PrerenderHandle* prerender_handle) {
  LinkPrerender* prerender = FindByPrerenderHandle(prerender_handle);
  if (!prerender)
    return;

  mojo::AssociatedRemote<chrome::mojom::PrerenderDispatcher>
      prerender_dispatcher =
          GetPrerenderDispatcher(prerender->launcher_child_id);
  if (prerender_dispatcher)
    prerender_dispatcher->PrerenderStop(prerender->prerender_id);

  RemovePrerender(prerender);
  StartPrerenders();
}

void PrerenderLinkManager::OnPrerenderNetworkBytesChanged(
    PrerenderHandle* prerender_handle) {}

}  // namespace prerender
