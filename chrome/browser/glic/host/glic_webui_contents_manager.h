// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_WEBUI_CONTENTS_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_WEBUI_CONTENTS_MANAGER_H_

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/glic_web_contents_manager.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

namespace glic {
class Host;

// Implementation of GlicWebContentsManager that hosts the guest client inside
// a WebUI <webview> container (chrome://glic).
class GlicWebUIContentsManager : public content::WebContentsObserver,
                                 public GlicWebContentsManager {
 public:
  // `initially_hidden` value is only relevant when
  // `kGlicGuestContentsVisibilityState` flag is enabled, otherwise the default
  // value is used (i.e. false).
  GlicWebUIContentsManager(Profile* profile, bool initially_hidden);
  ~GlicWebUIContentsManager() override;
  GlicWebUIContentsManager(const GlicWebUIContentsManager&) = delete;
  GlicWebUIContentsManager& operator=(const GlicWebUIContentsManager&) = delete;

  // GlicWebContentsManager impl.
  void AttachToHost(Host* host) override;
  void SetVisibility(content::Visibility visibility) override;
  content::WebContents* web_contents() const override;
  void OnActuatingChanged(bool actuating) override;
  void OnTaskTabsVisibilityChanged(bool has_visible_tab) override;
  std::unique_ptr<content::WebContents> ReleaseWebContents() override;
  void ReclaimWebContents(
      std::unique_ptr<content::WebContents> web_contents) override;
  base::CallbackListSubscription RegisterWebContentsChangedCallback(
      WebContentsChangedCallback callback) override;
  GlicWebClientManager* web_client_manager() override;
  bool IsCrashed() const override;

 private:
  // content::WebContentsObserver:
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryMainDocumentElementAvailable() override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void WebContentsDestroyed() override;
  void UpdateActuationTracker();

  const base::TimeTicks creation_time_ = base::TimeTicks::Now();
  base::TimeTicks navigation_commit_time_;
  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<content::WebContents> web_contents_ptr_ = nullptr;
  const raw_ptr<Profile> profile_;
  // Raw pointer to the host this UI is attached to. This object is not owned
  // by GlicUI. Its lifetime is managed by GlicInstanceImpl (multi-instance),
  // which owns Host.
  raw_ptr<Host> host_ = nullptr;

  base::ScopedClosureRunner webui_capture_runner_;
  base::ScopedClosureRunner guest_capture_runner_;

  bool is_actuating_ = false;
  bool is_actuating_on_visible_tab_ = false;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_WEBUI_CONTENTS_MANAGER_H_
