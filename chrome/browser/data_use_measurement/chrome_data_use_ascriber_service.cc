// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/chrome_data_use_ascriber_service.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_ascriber.h"
#include "chrome/browser/io_thread.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace {

data_use_measurement::ChromeDataUseAscriber* InitOnIOThread(
    IOThread* io_thread) {
  DCHECK(io_thread);
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  return io_thread->globals()->data_use_ascriber.get();
}

// Returns the top most parent of |render_frame_host|.
content::RenderFrameHost* GetMainFrame(
    content::RenderFrameHost* render_frame_host) {
  content::RenderFrameHost* render_main_frame_host = render_frame_host;
  while (render_main_frame_host->GetParent())
    render_main_frame_host = render_main_frame_host->GetParent();
  return render_main_frame_host;
}

}  // namespace

namespace data_use_measurement {

ChromeDataUseAscriberService::ChromeDataUseAscriberService()
    : ascriber_(nullptr), is_initialized_(false) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Skip IO thread initialization if there is no IO thread. This check is
  // required because unit tests that do no set up an IO thread can cause this
  // code to execute.
  if (!g_browser_process->io_thread()) {
    is_initialized_ = true;
    return;
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {content::BrowserThread::IO},
      base::Bind(&InitOnIOThread, g_browser_process->io_thread()),
      base::Bind(&ChromeDataUseAscriberService::SetDataUseAscriber,
                 base::Unretained(this)));
}

ChromeDataUseAscriberService::~ChromeDataUseAscriberService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void ChromeDataUseAscriberService::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!is_initialized_) {
    pending_frames_queue_.push_back(render_frame_host);
    return;
  }

  if (!ascriber_)
    return;

  int main_render_process_id = -1;
  int main_render_frame_id = -1;
  content::RenderFrameHost* main_frame = GetMainFrame(render_frame_host);
  if (main_frame != render_frame_host) {
    main_render_process_id = main_frame->GetProcess()->GetID();
    main_render_frame_id = main_frame->GetRoutingID();
  }

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&ChromeDataUseAscriber::RenderFrameCreated,
                     base::Unretained(ascriber_),
                     render_frame_host->GetProcess()->GetID(),
                     render_frame_host->GetRoutingID(), main_render_process_id,
                     main_render_frame_id));
}

void ChromeDataUseAscriberService::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!is_initialized_) {
    // While remove() is a O(n) operation, the pending queue is not expected
    // to have a significant number of elements.
    DCHECK_GE(50u, pending_frames_queue_.size());
    pending_frames_queue_.remove(render_frame_host);
    return;
  }

  if (!ascriber_)
    return;

  int main_render_process_id = -1;
  int main_render_frame_id = -1;
  content::RenderFrameHost* main_frame = GetMainFrame(render_frame_host);
  if (main_frame != render_frame_host) {
    main_render_process_id = main_frame->GetProcess()->GetID();
    main_render_frame_id = main_frame->GetRoutingID();
  }

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&ChromeDataUseAscriber::RenderFrameDeleted,
                     base::Unretained(ascriber_),
                     render_frame_host->GetProcess()->GetID(),
                     render_frame_host->GetRoutingID(), main_render_process_id,
                     main_render_frame_id));
}

void ChromeDataUseAscriberService::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!navigation_handle->IsInMainFrame())
    return;

  if (!ascriber_)
    return;

  content::WebContents* web_contents = navigation_handle->GetWebContents();
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&ChromeDataUseAscriber::ReadyToCommitMainFrameNavigation,
                     base::Unretained(ascriber_),
                     navigation_handle->GetGlobalRequestID(),
                     web_contents->GetMainFrame()->GetProcess()->GetID(),
                     web_contents->GetMainFrame()->GetRoutingID()));
}

void ChromeDataUseAscriberService::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!navigation_handle->IsInMainFrame())
    return;

  if (!ascriber_)
    return;

  content::WebContents* web_contents = navigation_handle->GetWebContents();
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&ChromeDataUseAscriber::DidFinishMainFrameNavigation,
                     base::Unretained(ascriber_),
                     web_contents->GetMainFrame()->GetProcess()->GetID(),
                     web_contents->GetMainFrame()->GetRoutingID(),
                     navigation_handle->GetURL(),
                     navigation_handle->IsSameDocument(),
                     navigation_handle->HasCommitted()
                         ? navigation_handle->GetPageTransition()
                         : ui::PAGE_TRANSITION_CORE_MASK,
                     base::TimeTicks::Now()));
}

void ChromeDataUseAscriberService::DidFinishLoad(
    content::RenderFrameHost* main_render_frame_host,
    const GURL& validated_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!ascriber_)
    return;

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&ChromeDataUseAscriber::DidFinishLoad,
                     base::Unretained(ascriber_),
                     main_render_frame_host->GetProcess()->GetID(),
                     main_render_frame_host->GetRoutingID(), validated_url));
}

void ChromeDataUseAscriberService::SetDataUseAscriber(
    ChromeDataUseAscriber* ascriber) {
  DCHECK(!is_initialized_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ascriber_ = ascriber;
  is_initialized_ = true;

  for (auto* it : pending_frames_queue_) {
    RenderFrameCreated(it);
    if (pending_visible_main_frames_.find(it) !=
        pending_visible_main_frames_.end()) {
      WasShownOrHidden(it, true);
    }
  }
  pending_frames_queue_.clear();
  pending_visible_main_frames_.clear();
}

void ChromeDataUseAscriberService::WasShownOrHidden(
    content::RenderFrameHost* main_render_frame_host,
    bool visible) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!ascriber_) {
    if (visible)
      pending_visible_main_frames_.insert(main_render_frame_host);
    else
      pending_visible_main_frames_.erase(main_render_frame_host);
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&ChromeDataUseAscriber::WasShownOrHidden,
                     base::Unretained(ascriber_),
                     main_render_frame_host->GetProcess()->GetID(),
                     main_render_frame_host->GetRoutingID(), visible));
}

void ChromeDataUseAscriberService::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  if (!ascriber_)
    return;

  if (old_host) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(
            &ChromeDataUseAscriber::RenderFrameHostChanged,
            base::Unretained(ascriber_), old_host->GetProcess()->GetID(),
            old_host->GetRoutingID(), new_host->GetProcess()->GetID(),
            new_host->GetRoutingID()));
  }
}

}  // namespace data_use_measurement
