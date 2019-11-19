// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_MESSAGE_FILTER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_MESSAGE_FILTER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"
#include "url/origin.h"

class Profile;
struct PrerenderAttributes;

namespace content {
struct Referrer;
}

namespace gfx {
class Size;
}

namespace IPC {
class Message;
}

namespace prerender {

class PrerenderLinkManager;
class PrerenderManager;

class PrerenderMessageFilter : public content::BrowserMessageFilter {
 public:
  PrerenderMessageFilter(int render_process_id, Profile* profile);

  static void EnsureShutdownNotifierFactoryBuilt();

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<PrerenderMessageFilter>;

  ~PrerenderMessageFilter() override;

  // Overridden from content::BrowserMessageFilter.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;
  void OnChannelClosing() override;
  void OnDestruct() const override;

  void OnAddPrerender(int prerender_id,
                      const PrerenderAttributes& attributes,
                      const content::Referrer& referrer,
                      const url::Origin& initiator_origin,
                      const gfx::Size& size,
                      int render_view_route_id);
  void OnCancelPrerender(int prerender_id);
  void OnAbandonPrerender(int prerender_id);
  void OnPrefetchFinished();

  void ShutdownOnUIThread();

  void OnChannelClosingInUIThread();

  PrerenderManager* prerender_manager_;

  const int render_process_id_;

  PrerenderLinkManager* prerender_link_manager_;

  std::unique_ptr<KeyedServiceShutdownNotifier::Subscription>
      shutdown_notifier_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderMessageFilter);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MESSAGE_FILTER_H_

