// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_WEBUI_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_GLIC_HOST_WEBUI_CONTENTS_CONTAINER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "content/public/browser/web_contents_observer.h"

namespace glic {
class GlicWindowController;

// Owns the `WebContents` that houses the chrome://glic WebUI.
class WebUIContentsContainer : public content::WebContentsObserver {
 public:
  // `initially_hidden` value is only relevant when
  // `kGlicGuestContentsVisibilityState` flag is enabled, otherwise the default
  // value is used (i.e. false).
  WebUIContentsContainer(Profile* profile,
                         GlicWindowController* glic_window_controller,
                         bool initially_hidden);
  ~WebUIContentsContainer() override;

  WebUIContentsContainer(const WebUIContentsContainer&) = delete;
  WebUIContentsContainer& operator=(const WebUIContentsContainer&) = delete;

 private:
  // content::WebContentsObserver:
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  ScopedProfileKeepAlive profile_keep_alive_;
  const std::unique_ptr<content::WebContents> web_contents_;
  // GlicWindowController owns this.
  const raw_ptr<GlicWindowController> glic_window_controller_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_WEBUI_CONTENTS_CONTAINER_H_
