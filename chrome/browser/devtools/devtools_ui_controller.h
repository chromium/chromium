// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_UI_CONTROLLER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_UI_CONTROLLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/focus/external_focus_tracker.h"

class ContentsContainerView;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class WebView;
}  // namespace views

// DevtoolsUIController handles managing the state of the devtools views.
class DevtoolsUIController {
 public:
  explicit DevtoolsUIController(
      std::vector<ContentsContainerView*> contents_container_views);
  ~DevtoolsUIController();

  void TearDown();

  bool UpdateDevtools(ContentsContainerView* contents_container,
                      content::WebContents* web_contents,
                      bool update_devtools_web_contents);

 private:
  // Controller for a single DevtoolsWebView owned by |contents_container_view|.
  // Manages updating the devtools window.
  class DevtoolsWebViewController {
   public:
    explicit DevtoolsWebViewController(
        ContentsContainerView* contents_container_view);
    DevtoolsWebViewController(const DevtoolsWebViewController&) = delete;
    DevtoolsWebViewController& operator=(const DevtoolsWebViewController&) =
        delete;
    ~DevtoolsWebViewController();

    void OnWebContentsAttached(views::WebView* web_view);
    void OnWebContentsDetached(views::WebView* web_view);

    // Updates devtools window for given contents. This method will show docked
    // devtools window for inspected |web_contents| that has docked devtools
    // and hide it for null or not inspected |web_contents|. It will also make
    // sure devtools window size and position are restored for given tab.
    // This method will not update actual DevTools WebContents, if not
    // |update_devtools_web_contents|. Returns true if devtools changes
    // visibility or the resizing strategy is updated.
    bool UpdateDevtools(content::WebContents* web_contents,
                        bool update_devtools_web_contents);

    void RestoreFocus();

   private:
    raw_ptr<ContentsContainerView> contents_container_view_;
    // Tracks and stores the last focused view which is not the
    // devtools_web_view_ or any of its children. Used to restore focus once
    // the devtools_web_view_ is hidden.
    std::unique_ptr<views::ExternalFocusTracker> devtools_focus_tracker_;

    base::CallbackListSubscription web_contents_attached_subscription_;
    base::CallbackListSubscription web_contents_detached_subscription_;
  };

  std::map<ContentsContainerView*, std::unique_ptr<DevtoolsWebViewController>>
      devtools_web_view_controllers_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_UI_CONTROLLER_H_
