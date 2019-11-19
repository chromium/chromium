// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_message_filter.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/task/post_task.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_link_manager.h"
#include "chrome/browser/prerender/prerender_link_manager_factory.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/prerender_messages.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/child_process_security_policy.h"

using content::BrowserThread;

namespace prerender {

namespace {

class PrerenderMessageFilterShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static PrerenderMessageFilterShutdownNotifierFactory* GetInstance() {
    return base::Singleton<
        PrerenderMessageFilterShutdownNotifierFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<
      PrerenderMessageFilterShutdownNotifierFactory>;

  PrerenderMessageFilterShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "PrerenderMessageFilter") {
    DependsOn(PrerenderLinkManagerFactory::GetInstance());
  }
  ~PrerenderMessageFilterShutdownNotifierFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(PrerenderMessageFilterShutdownNotifierFactory);
};

}  // namespace

PrerenderMessageFilter::PrerenderMessageFilter(int render_process_id,
                                               Profile* profile)
    : BrowserMessageFilter(PrerenderMsgStart),
      prerender_manager_(
          PrerenderManagerFactory::GetForBrowserContext(profile)),
      render_process_id_(render_process_id),
      prerender_link_manager_(
          PrerenderLinkManagerFactory::GetForProfile(profile)) {
  shutdown_notifier_ =
      PrerenderMessageFilterShutdownNotifierFactory::GetInstance()
          ->Get(profile)
          ->Subscribe(base::Bind(&PrerenderMessageFilter::ShutdownOnUIThread,
                                 base::Unretained(this)));
}

PrerenderMessageFilter::~PrerenderMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

// static
void PrerenderMessageFilter::EnsureShutdownNotifierFactoryBuilt() {
  PrerenderMessageFilterShutdownNotifierFactory::GetInstance();
}

bool PrerenderMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrerenderMessageFilter, message)
    IPC_MESSAGE_HANDLER(PrerenderHostMsg_AddLinkRelPrerender, OnAddPrerender)
    IPC_MESSAGE_HANDLER(
        PrerenderHostMsg_CancelLinkRelPrerender, OnCancelPrerender)
    IPC_MESSAGE_HANDLER(
        PrerenderHostMsg_AbandonLinkRelPrerender, OnAbandonPrerender)
    IPC_MESSAGE_HANDLER(PrerenderHostMsg_PrefetchFinished, OnPrefetchFinished)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void PrerenderMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, content::BrowserThread::ID* thread) {
  if (message.type() == PrerenderHostMsg_AddLinkRelPrerender::ID ||
      message.type() == PrerenderHostMsg_CancelLinkRelPrerender::ID ||
      message.type() == PrerenderHostMsg_AbandonLinkRelPrerender::ID ||
      message.type() == PrerenderHostMsg_PrefetchFinished::ID) {
    *thread = BrowserThread::UI;
  }
}

void PrerenderMessageFilter::OnChannelClosing() {
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&PrerenderMessageFilter::OnChannelClosingInUIThread,
                     this));
}

void PrerenderMessageFilter::OnDestruct() const {
  // |shutdown_notifier_| needs to be destroyed on the UI thread.
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

void PrerenderMessageFilter::OnAddPrerender(
    int prerender_id,
    const PrerenderAttributes& attributes,
    const content::Referrer& referrer,
    const url::Origin& initiator_origin,
    const gfx::Size& size,
    int render_view_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!prerender_link_manager_)
    return;
  if (!initiator_origin.opaque() &&
      !content::ChildProcessSecurityPolicy::GetInstance()
           ->CanAccessDataForOrigin(render_process_id_,
                                    initiator_origin.GetURL())) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::PMF_INVALID_INITIATOR_ORIGIN);
    return;
  }
  prerender_link_manager_->OnAddPrerender(
      render_process_id_, prerender_id, attributes.url, attributes.rel_types,
      referrer, initiator_origin, size, render_view_route_id);
}

void PrerenderMessageFilter::OnCancelPrerender(
    int prerender_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!prerender_link_manager_)
    return;
  prerender_link_manager_->OnCancelPrerender(render_process_id_, prerender_id);
}

void PrerenderMessageFilter::OnAbandonPrerender(
    int prerender_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!prerender_link_manager_)
    return;
  prerender_link_manager_->OnAbandonPrerender(render_process_id_, prerender_id);
}

void PrerenderMessageFilter::OnPrefetchFinished() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(prerender_manager_);
  // Kill the process doing the prefetch. Only one prefetch per renderer is
  // possible, also prefetches are not shared with other renderer processes.
  if (prerender_manager_) {
    PrerenderContents* prerender_contents =
        prerender_manager_->GetPrerenderContentsForProcess(render_process_id_);
    if (prerender_contents)
      prerender_contents->Destroy(FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  }
}

void PrerenderMessageFilter::ShutdownOnUIThread() {
  prerender_link_manager_ = nullptr;
  shutdown_notifier_.reset();
}

void PrerenderMessageFilter::OnChannelClosingInUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!prerender_link_manager_)
    return;
  prerender_link_manager_->OnChannelClosing(render_process_id_);
}

}  // namespace prerender

