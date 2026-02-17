// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WEB_VIEW_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WEB_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/media_stream_request.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"

class BrowserWindowInterface;

namespace content {
class BrowserContext;
class NavigationHandle;
class WebContents;
struct OpenURLParams;
}  // namespace content

namespace input {
struct NativeWebKeyboardEvent;
}  // namespace input

namespace web_modal {
class WebContentsModalDialogHost;
}  // namespace web_modal

namespace contextual_tasks {

class ContextualTasksWebView
    : public views::WebView,
      public web_modal::WebContentsModalDialogManagerDelegate {
 public:
  explicit ContextualTasksWebView(content::BrowserContext* browser_context);
  ~ContextualTasksWebView() override;

  base::WeakPtr<ContextualTasksWebView> GetWeakPtr();

  // views::WebView:
  void SetWebContents(content::WebContents* wc) override;

  // content::WebContentsDelegate:
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;

  // web_modal::WebContentsModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost(
      content::WebContents* web_contents) override;

 private:
  // Attach a modal dialog manager to a WebContents so that dialogs can be
  // displayed correctly while in the side panel.
  void AttachWebContentsModalDialogManager(content::WebContents* web_contents);

  // Detach the modal dialog manager from the provided WebContents. This should
  // happen when the contents is detached from the side panel.
  void DetachWebContentsModalDialogManager(content::WebContents* web_contents);

  BrowserWindowInterface* GetBrowser();

  // A handler to handle unhandled keyboard messages coming back from the
  // renderer process.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  base::WeakPtrFactory<ContextualTasksWebView> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WEB_VIEW_H_
