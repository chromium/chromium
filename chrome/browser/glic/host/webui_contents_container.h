// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_WEBUI_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_GLIC_HOST_WEBUI_CONTENTS_CONTAINER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

namespace glic {
class Host;

class WebUIContentsContainer {
 public:
  WebUIContentsContainer();
  virtual ~WebUIContentsContainer();

  // Attaches this container's WebContents to the provided Host. This must be
  // called exactly once.
  virtual void AttachToHost(Host* host) = 0;
  virtual content::WebContents* web_contents() const = 0;
  base::TimeTicks creation_time() const { return creation_time_; }

 protected:
  const base::TimeTicks creation_time_;
};

// Owns the `WebContents` that houses the chrome://glic WebUI.
class WebUIContentsContainerImpl : public content::WebContentsObserver,
                                   public WebUIContentsContainer {
 public:
  // `initially_hidden` value is only relevant when
  // `kGlicGuestContentsVisibilityState` flag is enabled, otherwise the default
  // value is used (i.e. false).
  WebUIContentsContainerImpl(Profile* profile, bool initially_hidden);
  ~WebUIContentsContainerImpl() override;
  WebUIContentsContainerImpl(const WebUIContentsContainerImpl&) = delete;
  WebUIContentsContainerImpl& operator=(const WebUIContentsContainerImpl&) =
      delete;

  // WebUIContentsContainer impl.
  void AttachToHost(Host* host) override;
  content::WebContents* web_contents() const override;

 private:
  // content::WebContentsObserver:
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryMainDocumentElementAvailable() override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

  base::TimeTicks navigation_commit_time_;
  ScopedProfileKeepAlive profile_keep_alive_;
  const std::unique_ptr<content::WebContents> web_contents_;
  const raw_ptr<Profile> profile_;
  // Raw pointer to the host this UI is attached to. This object is not owned
  // by GlicUI. Its lifetime is managed by GlicInstanceImpl (multi-instance),
  // which owns Host.
  raw_ptr<Host> host_ = nullptr;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_WEBUI_CONTENTS_CONTAINER_H_
