// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_PARENT_ACCESS_VIEW_H_
#define CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_PARENT_ACCESS_VIEW_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/timer/timer.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class WebView;
}  // namespace views

namespace content {
class WebContents;
class BrowserContext;
}  // namespace content

// Helper class that closes the the parent approval dialog, if we fail
// to load within a timeout the PACP contents that need to be displayed.
class DialogContentLoadWithTimeoutObserver
    : public content::WebContentsObserver {
 public:
  DialogContentLoadWithTimeoutObserver(
      content::WebContents* web_contents,
      const GURL pacp_url,
      base::OnceClosure show_view_and_destroy_timer_callback,
      base::OnceClosure cancel_flow_on_timeout_callback);
  DialogContentLoadWithTimeoutObserver() = delete;
  ~DialogContentLoadWithTimeoutObserver() override;
  DialogContentLoadWithTimeoutObserver(
      const DialogContentLoadWithTimeoutObserver&) = delete;
  DialogContentLoadWithTimeoutObserver& operator=(
      const DialogContentLoadWithTimeoutObserver&) = delete;

 private:
  // WebContentsObserver overrides:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  const GURL pacp_url_;
  base::OneShotTimer initial_load_timer_;
  base::OnceClosure show_view_and_destroy_timer_callback_;
};

using WebContentsObservationCallback =
    base::OnceCallback<void(content::WebContents*)>;

// Implements a View to display the Parent Access Widget (PACP).
// The view contains a WebView which loads the PACP url.
class ParentAccessView : public views::View,
                         public views::WidgetObserver,
                         public content::WebContentsDelegate {
  METADATA_HEADER(ParentAccessView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kErrorDialogBackButtonElementId);

  ParentAccessView(content::BrowserContext* context,
                   base::OnceClosure dialog_result_reset_callback);
  ~ParentAccessView() override;

  // Creates and opens a view that displays the Parent Access widget (PACP).
  static base::WeakPtr<ParentAccessView> ShowParentAccessDialog(
      content::WebContents* web_contents,
      const GURL& target_url,
      const supervised_user::FilteringBehaviorReason& filtering_reason,
      WebContentsObservationCallback web_contents_observation_cb,
      base::OnceClosure abort_dialog_callback,
      base::OnceClosure dialog_result_reset_callback);

  base::WeakPtr<ParentAccessView> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Closes the widget that hosts this view.
  // Results in destructing the present view and its widget.
  void CloseView();
  void DisplayErrorMessage(content::WebContents* web_contents);

  // Exposed for testing.
  views::WebView* GetWebViewForTesting() { return web_view_.get(); }
  views::View* GetErrorViewForTesting() { return error_view_.get(); }

 private:
  // Initialize ParentAccessView's web_view_ element.
  void Initialize(const GURL& pacp_url, int corner_radius);
  void ShowNativeView();
  content::WebContents* GetWebViewContents();
  // views::WidgetObserver implementation:
  void OnWidgetThemeChanged(views::Widget* widget) override;

  // views::View override:
  void ChildPreferredSizeChanged(View* child) override;

  void OnWidgetClose(views::Widget::ClosedReason closed_reason);

  // content::WebContentsDelegate override:
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  void ResizeDueToAutoResize(content::WebContents* web_contents,
                             const gfx::Size& new_size) override;

  void ShowWebViewAndDestroyTimeoutObserver();

  void UpdateDialogBorderAndChildrenBackgroundColors();

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  base::OnceClosure dialog_result_reset_callback_;
  base::ScopedMultiSourceObservation<views::Widget, views::WidgetObserver>
      widget_observations_{this};

  std::unique_ptr<DialogContentLoadWithTimeoutObserver>
      content_loader_timeout_observer_;
  bool is_initialized_ = false;
  int corner_radius_ = 0;
  // Web view that displays the PACP widget content.
  raw_ptr<views::WebView> web_view_ = nullptr;
  // View used to display an error message in case of failure.
  raw_ptr<views::View> error_view_ = nullptr;
  // Assumes ownership of a removed child view.
  std::unique_ptr<views::View> removed_view_holder_;

  base::WeakPtrFactory<ParentAccessView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_PARENT_ACCESS_VIEW_H_
