// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_GUEST_OBSERVER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_GUEST_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace glic {
class Host;
class GlicWebContentsManager;

// Observes the Glic guest `WebContents`, enforcing policies (autoplay, Mojo JS
// bindings) and maintaining container and host associations for Mojo interface
// routing.
class GlicGuestObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<GlicGuestObserver> {
 public:
  static void CreateForWebContents(content::WebContents& web_contents,
                                   GlicWebContentsManager& contents_manager);

  ~GlicGuestObserver() override;

  GlicGuestObserver(const GlicGuestObserver&) = delete;
  GlicGuestObserver& operator=(const GlicGuestObserver&) = delete;

  Host* host() const { return host_; }
  void set_host(Host* host) { host_ = host; }

  GlicWebContentsManager& contents_manager() const {
    return *contents_manager_;
  }

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  explicit GlicGuestObserver(content::WebContents& web_contents,
                             GlicWebContentsManager& contents_manager);
  friend class content::WebContentsUserData<GlicGuestObserver>;

  void GrantAutoplayPermissions(content::NavigationHandle* navigation_handle);
  void MaybeEnableMojoJsBindings(content::RenderFrameHost* render_frame_host);
  void MaybeEnableMojoJsBindings(content::NavigationHandle* navigation_handle);
  void MaybeSetBackgroundColor(content::RenderFrameHost* render_frame_host);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  raw_ref<GlicWebContentsManager> contents_manager_;
  raw_ptr<Host> host_ = nullptr;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_GUEST_OBSERVER_H_
