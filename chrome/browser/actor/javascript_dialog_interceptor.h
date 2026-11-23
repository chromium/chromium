// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_JAVASCRIPT_DIALOG_INTERCEPTOR_H_
#define CHROME_BROWSER_ACTOR_JAVASCRIPT_DIALOG_INTERCEPTOR_H_

#include <cstddef>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/common/actor/action_result.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace actor {

// Intercepts JavaScript dialogs (`window.alert`, `window.confirm`,
// `window.prompt`) at the browser layer while an active actor task is
// controlling a WebContents.
//
// Rather than blocking the renderer on a modal UI dialog (or silently dropping
// the text when a tab is backgrounded or headless), this manager synchronously
// auto-dismisses each dialog and buffers `{type, dialog_message}` for dialogs
// from the primary main frame or same-origin non-opaque subframes in the
// primary page so ExecutionEngine can attach them to the action receipt.
// Dialogs from opaque-origin or cross-origin subframes (e.g. sandboxed iframes)
// are still auto-dismissed without being recorded.
//
// `beforeunload` dialogs are not intercepted here; they continue to delegate to
// the underlying TabModalDialogManager (or auto-proceed on headless
// WebContents), keeping `kGlicSkipBeforeUnloadDialogAndNavigate` orthogonal.
//
// Guest-view frames routed via
// `GuestPageHolder::Delegate::GuestGetJavascriptDialogManager()` (such as
// `<webview>`) are out of scope: actor actuation only targets regular web
// pages, `MimeHandlerViewGuest` (e.g. PDF viewer) delegates back to the owner
// WebContents's delegate, and `WebViewGuest` does not display Chrome tab-modal
// UI. When a non-MPArch guest WebContents (such as `MimeHandlerViewGuest`)
// borrows its owner tab's interceptor and passes the guest `WebContents*`,
// regular dialogs are auto-dismissed without being recorded (matching MPArch
// guests and untrusted subframes), while `beforeunload`, handle, and cancel
// calls look up the owner's fallback manager and forward the original
// `WebContents*`.
class JavaScriptDialogInterceptor
    : public content::WebContentsUserData<JavaScriptDialogInterceptor>,
      public content::JavaScriptDialogManager {
 public:
  // Maximum number of dialogs buffered per WebContents between drains. Dialogs
  // beyond this cap are still auto-dismissed so the renderer never blocks.
  static constexpr size_t kMaxInterceptedDialogs = 50;

  // Maximum UTF-8 byte length stored per dialog message.
  static constexpr size_t kMaxDialogMessageBytes = 16 * 1024;

  JavaScriptDialogInterceptor(const JavaScriptDialogInterceptor&) = delete;
  JavaScriptDialogInterceptor& operator=(const JavaScriptDialogInterceptor&) =
      delete;
  ~JavaScriptDialogInterceptor() override;

  // Returns this WebContents' interceptor if `kActorInterceptJavaScriptDialogs`
  // is enabled and an active actor task is currently controlling
  // `web_contents`, creating it on demand. Otherwise returns nullptr so the
  // delegate falls back to its regular JavaScriptDialogManager.
  static content::JavaScriptDialogManager* MaybeGetForWebContents(
      content::WebContents* web_contents);

  // Drains and returns all dialogs intercepted on `web_contents` since the
  // previous call, in the order they fired.
  static std::vector<InterceptedJavaScriptDialog> TakeInterceptedDialogs(
      content::WebContents& web_contents);

  // Discards any buffered dialogs on `web_contents` without creating an
  // interceptor if none is attached.
  static void ClearInterceptedDialogs(content::WebContents& web_contents);

  // Dismisses any active or pending TabModalDialogManager dialog on
  // `web_contents` when `kActorInterceptJavaScriptDialogs` is enabled, without
  // affecting app-modal `beforeunload` dialogs. Called when a WebContents
  // enters actor control so a pre-existing modal dialog does not leave the
  // renderer blocked before the first actor action runs.
  static void DismissExistingTabModalDialog(content::WebContents& web_contents);

  // Removes the interceptor (and any buffered dialogs) from `web_contents`.
  static void RemoveForWebContents(content::WebContents& web_contents);

  // Overrides the fallback JavaScriptDialogManager used for beforeunload and
  // cancellation delegation in unit tests. Caller retains ownership.
  void SetFallbackDialogManagerForTesting(
      content::JavaScriptDialogManager* fallback);

  // content::JavaScriptDialogManager:
  void RunJavaScriptDialog(content::WebContents* web_contents,
                           content::RenderFrameHost* render_frame_host,
                           content::JavaScriptDialogType dialog_type,
                           const std::u16string& message_text,
                           const std::u16string& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override;
  void RunBeforeUnloadDialog(content::WebContents* web_contents,
                             content::RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override;
  bool HandleJavaScriptDialog(content::WebContents* web_contents,
                              bool accept,
                              const std::u16string* prompt_override) override;
  void CancelDialogs(content::WebContents* web_contents,
                     bool reset_state) override;

 private:
  friend class content::WebContentsUserData<JavaScriptDialogInterceptor>;

  explicit JavaScriptDialogInterceptor(content::WebContents* web_contents);

  content::JavaScriptDialogManager* GetFallbackDialogManager();

  std::vector<InterceptedJavaScriptDialog> intercepted_dialogs_;
  raw_ptr<content::JavaScriptDialogManager> fallback_for_testing_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_JAVASCRIPT_DIALOG_INTERCEPTOR_H_
