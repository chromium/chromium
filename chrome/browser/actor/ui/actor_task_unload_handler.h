// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_TASK_UNLOAD_HANDLER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_TASK_UNLOAD_HANDLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/unload_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}

namespace views {
class BubbleDialogModelHost;
class Widget;
}  // namespace views

namespace actor {

// A custom Views dialog shown to confirm closing a tab while an Actor task is
// active. It is tab-modal.
class ActorTaskTabCloseConfirmDialog {
 public:
  ActorTaskTabCloseConfirmDialog() = delete;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kViewId);

  using CloseCallback = base::OnceCallback<void(bool /* confirmed */)>;

  // Checks if the given `web_contents` is currently being actuated by an Actor
  // task. Shows the modal dialog and returns the widget if the dialog should be
  // shown.
  static std::unique_ptr<views::Widget> ShowModalIfActuating(
      content::WebContents* web_contents,
      CloseCallback callback);

  // Checks if the given `web_contents` is currently being actuated by an Actor
  // task. Returns the delegate if the dialog should be shown. If not null,
  // the caller is responsible for creating and owning the widget.
  static std::unique_ptr<views::BubbleDialogModelHost>
  CreateDelegateIfActuating(content::WebContents* web_contents,
                            CloseCallback callback);

  // Returns true if the dialog should be shown for the given web_contents.
  static bool ShouldShow(content::WebContents* web_contents);

  // Creates the dialog delegate for the given `web_contents`.
  static std::unique_ptr<views::BubbleDialogModelHost> CreateDelegate(
      content::WebContents* web_contents,
      CloseCallback callback);

  // Test hook to cause the dialog to always be shown, regardless of whether
  // there really is an active Actor task.
  static void SetShowAlwaysReturnsTrueForTesting(bool always_show);
  static bool ShouldAlwaysShowForTesting();

  // Test hook to completely suppress showing the dialog during test teardown.
  static void SetSuppressForTesting(bool suppress);
  static bool ShouldSuppressForTesting();
};

class ActorTaskUnloadHandler : public UnloadController::TabUnloadHandler {
 public:
  ActorTaskUnloadHandler();
  ~ActorTaskUnloadHandler() override;

  bool ShouldSkipBeforeUnload(content::WebContents* contents) override;
  bool ShouldShowCustomConfirmation(content::WebContents* contents) override;
  bool ShowCustomConfirmation(
      content::WebContents* contents,
      base::OnceCallback<void(bool /* confirmed */)> on_closed) override;
  views::Widget* GetActiveDialogWidgetForTesting() const;

 private:
  std::unique_ptr<views::Widget> owned_widget_;
  base::WeakPtr<views::Widget> active_widget_;
  base::WeakPtrFactory<ActorTaskUnloadHandler> weak_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_TASK_UNLOAD_HANDLER_H_
