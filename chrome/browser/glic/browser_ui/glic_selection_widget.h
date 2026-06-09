// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SELECTION_WIDGET_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SELECTION_WIDGET_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"


namespace views {
class Widget;
}

namespace glic {

class GlicSelectionWidgetDelegate : public views::BubbleDialogDelegate {
 public:
  // Pure virtual interface implemented by the bridge to receive UI events.
  class ActionDelegate {
   public:
    virtual void OnAskGemini() = 0;
    virtual void OnCopy() = 0;
    virtual void OnCopyLink() = 0;
    virtual void OnPinToggled(bool is_pinned) = 0;
    virtual void OnDismiss() = 0;  // TODO(b/520398290): Remove OnDismiss.
    virtual void OnHideForThisSite() = 0;
    virtual void OnSettings() = 0;

   protected:
    virtual ~ActionDelegate() = default;
  };

  enum class MenuCommand {
    kHideForSite = 1,
    kSettings = 2,
  };

  GlicSelectionWidgetDelegate(ActionDelegate& action_delegate,
                              const gfx::Rect& anchor_rect,
                              const gfx::Rect& window_bounds,
                              const std::u16string& selected_text,
                              bool is_pinned);
  ~GlicSelectionWidgetDelegate() override;

  ActionDelegate& action_delegate() const { return *action_delegate_; }

  void TogglePinState();
  void UpdatePosition();

  views::ClientView* CreateClientView(views::Widget* widget) override;

  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;

  void UpdateCopyLinkButton(bool enabled);

 private:
  friend class GlicSelectionWidgetTest;

  void TriggerMenuCommandForTesting(int command_id);

  const raw_ref<ActionDelegate> action_delegate_;
  gfx::Rect original_anchor_rect_;
  gfx::Rect window_bounds_;
  bool is_pinned_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SELECTION_WIDGET_H_
