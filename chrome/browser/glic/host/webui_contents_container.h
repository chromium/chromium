// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_WEBUI_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_GLIC_HOST_WEBUI_CONTENTS_CONTAINER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

namespace glic {
class Host;

// Owns the `WebContents` that houses the chrome://glic WebUI.
class WebUIContentsContainer : public content::WebContentsObserver {
 public:
  // `initially_hidden` value is only relevant when
  // `kGlicGuestContentsVisibilityState` flag is enabled, otherwise the default
  // value is used (i.e. false).
  WebUIContentsContainer(Profile* profile, bool initially_hidden);
  ~WebUIContentsContainer() override;

  // Attaches this container's WebContents to the provided Host. This must be
  // called exactly once.
  void AttachToHost(Host* host);
  content::WebContents* web_contents() const { return web_contents_.get(); }
  WebUIContentsContainer(const WebUIContentsContainer&) = delete;
  WebUIContentsContainer& operator=(const WebUIContentsContainer&) = delete;

 private:
  // content::WebContentsObserver:
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  ScopedProfileKeepAlive profile_keep_alive_;
  const std::unique_ptr<content::WebContents> web_contents_;
  const raw_ptr<Profile> profile_;
  // Raw pointer to the host this UI is attached to. This object is not owned
  // by GlicUI. Its lifetime is managed by GlicKeyedService (single-instance) or
  // GlicInstanceImpl (multi-instance).
  //
  // In the single-instance path, `HostManager` (owned by `GlicKeyedService`)
  // owns `Host`s. `HostManager::Shutdown()` is called during
  // `GlicKeyedService::Shutdown()`, which destroys all hosts and thus their
  // associated WebUIs.
  //
  // In the multi-instance path, `GlicInstanceImpl` owns `Host`. The
  // `GlicInstanceImpl` calls `Shutdown()` on the `Host` in its destructor,
  // which destroys the WebUI (and thus this `GlicUI`), ensuring `host_`
  // outlives `this`.
  raw_ptr<Host> host_ = nullptr;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_WEBUI_CONTENTS_CONTAINER_H_
