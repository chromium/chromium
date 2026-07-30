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
    virtual void OnAskGeminiForQuery(const std::u16string& query) = 0;
    virtual void OnAskGeminiMoreAboutThis(
        const std::u16string& selected_text,
        const std::string& explanation_text) = 0;
    virtual void OnCopy() = 0;
    virtual void OnCopyLink() = 0;
    virtual void OnHide() = 0;
    virtual void OnSettings() = 0;
    virtual void OnOpenInSidePanel() = 0;
    virtual void OnWidgetClose() = 0;
    virtual bool IsInlineFulfillmentSupported() = 0;

   protected:
    virtual ~ActionDelegate() = default;
  };

  GlicSelectionWidgetDelegate(ActionDelegate& action_delegate,
                              const gfx::Rect& anchor_rect,
                              const gfx::Rect& window_bounds,
                              const std::u16string& selected_text);
  ~GlicSelectionWidgetDelegate() override;

  void ShowWidget();
  void CloseWidget();
  void OnWidgetClose(views::Widget::ClosedReason reason);

  ActionDelegate& action_delegate() const { return *action_delegate_; }

  void UpdatePosition();

  views::ClientView* CreateClientView(views::Widget* widget) override;

  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;

  void UpdateCopyLinkButton(bool enabled);
  void ShowInlineExplanation(const std::string& markdown_output,
                             bool is_complete,
                             const std::string& error_message);

 private:
  friend class GlicSelectionWidgetTest;

  const raw_ref<ActionDelegate> action_delegate_;
  gfx::Rect original_anchor_rect_;
  gfx::Rect window_bounds_;
  std::unique_ptr<views::Widget> widget_;
  base::WeakPtrFactory<GlicSelectionWidgetDelegate> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SELECTION_WIDGET_H_
