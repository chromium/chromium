// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WEBUI_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_GLIC_WEBUI_CONTENTS_CONTAINER_H_

#include <memory>

#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

namespace glic {
class GlicWindowController;

// Owns the `WebContents` that houses the chrome://glic WebUI.
class WebUIContentsContainer : public content::WebContentsDelegate {
 public:
  WebUIContentsContainer(Profile* profile,
                         GlicWindowController* glic_window_controller);
  ~WebUIContentsContainer() override;

  WebUIContentsContainer(const WebUIContentsContainer&) = delete;
  WebUIContentsContainer& operator=(const WebUIContentsContainer&) = delete;

  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  // Observes the outer and inner web contents.
  class WCObserver;
  friend class WCObserver;

  // content::WebContentsDelegate:
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;

  void InnerWebContentsAttached(content::WebContents* contents,
                                WCObserver* observer);
  void RendererCrashed(WCObserver* observer);

  ScopedProfileKeepAlive profile_keep_alive_;
  const std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<WCObserver> outer_wc_observer_;
  std::unique_ptr<WCObserver> inner_wc_observer_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  // GlicWindowController owns this.
  const raw_ptr<GlicWindowController> glic_window_controller_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WEBUI_CONTENTS_CONTAINER_H_
