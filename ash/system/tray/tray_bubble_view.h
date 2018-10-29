// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_BUBBLE_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_BUBBLE_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/mouse_watcher.h"

namespace aura {
class Env;
}

namespace ui {
class LayerOwner;
}

namespace views {
class BoxLayout;
class View;
class Widget;
}  // namespace views

namespace ash {

// Specialized bubble view for bubbles associated with a tray icon (e.g. the
// Ash status area). Mostly this handles custom anchor location and arrow and
// border rendering. This also has its own delegate for handling mouse events
// and other implementation specific details.
class ASH_EXPORT TrayBubbleView : public views::BubbleDialogDelegateView,
                                  public views::MouseWatcherListener {
 public:
  // AnchorAlignment determines to which side of the anchor the bubble will
  // align itself.
  enum AnchorAlignment {
    ANCHOR_ALIGNMENT_BOTTOM,
    ANCHOR_ALIGNMENT_LEFT,
    ANCHOR_ALIGNMENT_RIGHT,
  };

  class ASH_EXPORT Delegate {
   public:
    typedef TrayBubbleView::AnchorAlignment AnchorAlignment;

    Delegate() {}
    virtual ~Delegate();

    // Called when the view is destroyed. Any pointers to the view should be
    // cleared when this gets called.
    virtual void BubbleViewDestroyed();

    // Called when the mouse enters/exits the view.
    // Note: This event will only be called if the mouse gets actively moved by
    // the user to enter the view.
    virtual void OnMouseEnteredView();
    virtual void OnMouseExitedView();

    // Called from GetAccessibleNodeData(); should return the appropriate
    // accessible name for the bubble.
    virtual base::string16 GetAccessibleNameForBubble();

    // Should return true if extra keyboard accessibility is enabled.
    // TrayBubbleView will put focus on the default item if extra keyboard
    // accessibility is enabled.
    virtual bool ShouldEnableExtraKeyboardAccessibility();

    // Called when a bubble wants to hide/destroy itself (e.g. last visible
    // child view was closed).
    virtual void HideBubble(const TrayBubbleView* bubble_view);

    // Called to process the gesture events that happened on the TrayBubbleView.
    // Swiping down on the opened TrayBubbleView to close the bubble.
    virtual void ProcessGestureEventForBubble(ui::GestureEvent* event);

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Anchor mode being set at creation.
  enum class AnchorMode {
    // Anchor to |anchor_view|. This is the default.
    kView,
    // Anchor to |anchor_rect|. Used for anchoring to the shelf.
    kRect
  };

  struct ASH_EXPORT InitParams {
    InitParams();
    InitParams(const InitParams& other);
    Delegate* delegate = nullptr;
    gfx::NativeWindow parent_window = nullptr;
    View* anchor_view = nullptr;
    AnchorMode anchor_mode = AnchorMode::kView;
    // Only used if anchor_mode == AnchorMode::kRect.
    gfx::Rect anchor_rect;
    AnchorAlignment anchor_alignment = ANCHOR_ALIGNMENT_BOTTOM;
    int min_width = 0;
    int max_width = 0;
    int max_height = 0;
    bool close_on_deactivate = true;
    // Indicates whether tray bubble view is shown by click on the tray view.
    bool show_by_click = false;
    // If not provided, the bg color will be derived from the NativeTheme.
    base::Optional<SkColor> bg_color;
    base::Optional<int> corner_radius;
    bool has_shadow = true;
  };

  explicit TrayBubbleView(const InitParams& init_params);
  ~TrayBubbleView() override;

  // Returns whether a tray bubble is active.
  static bool IsATrayBubbleOpen();

  // Sets up animations, and show the bubble. Must occur after CreateBubble()
  // is called.
  void InitializeAndShowBubble();

  // Called whenever the bubble size or location may have changed.
  void UpdateBubble();

  // Sets the maximum bubble height and resizes the bubble.
  void SetMaxHeight(int height);

  // Sets the bottom padding that child views will be laid out within.
  void SetBottomPadding(int padding);

  // Sets the bubble width.
  void SetWidth(int width);

  // Returns the border insets. Called by TrayEventFilter.
  gfx::Insets GetBorderInsets() const;

  // Called when the delegate is destroyed. This must be called before the
  // delegate is actually destroyed. TrayBubbleView will do clean up in
  // ResetDelegate.
  void ResetDelegate();

  // Anchors the bubble to |anchor_view|.
  // Only eligible if anchor_mode == AnchorMode::kView.
  void ChangeAnchorView(views::View* anchor_view);

  // Anchors the bubble to |anchor_rect|. Exclusive with ChangeAnchorView().
  // Only eligible if anchor_mode == AnchorMode::kRect.
  void ChangeAnchorRect(const gfx::Rect& anchor_rect);

  // Change anchor alignment mode when anchoring either the rect or view.
  void ChangeAnchorAlignment(AnchorAlignment alignment);

  Delegate* delegate() { return delegate_; }

  void set_gesture_dragging(bool dragging) { is_gesture_dragging_ = dragging; }
  bool is_gesture_dragging() const { return is_gesture_dragging_; }

  // Overridden from views::WidgetDelegate.
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  bool WidgetHasHitTestMask() const override;
  void GetWidgetHitTestMask(gfx::Path* mask) const override;
  base::string16 GetAccessibleWindowTitle() const override;

  // Overridden from views::BubbleDialogDelegateView.
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* bubble_widget) const override;
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // Overridden from views::View.
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden from MouseWatcherListener
  void MouseMovedOutOfHost() override;

 protected:
  // Overridden from views::BubbleDialogDelegateView.
  int GetDialogButtons() const override;
  ax::mojom::Role GetAccessibleWindowRole() const override;
  void SizeToContents() override;

  // Overridden from views::View.
  void ChildPreferredSizeChanged(View* child) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

 private:
  // This reroutes receiving key events to the TrayBubbleView passed in the
  // constructor. TrayBubbleView is not activated by default. But we want to
  // activate it if user tries to interact it with keyboard. To capture those
  // key events in early stage, RerouteEventHandler installs this handler to
  // aura::Env. RerouteEventHandler also sends key events to ViewsDelegate to
  // process accelerator as menu is currently open.
  class RerouteEventHandler : public ui::EventHandler {
   public:
    RerouteEventHandler(TrayBubbleView* tray_bubble_view, aura::Env* aura_env);
    ~RerouteEventHandler() override;

    // Overridden from ui::EventHandler
    void OnKeyEvent(ui::KeyEvent* event) override;

   private:
    // TrayBubbleView to which key events are going to be rerouted. Not owned.
    TrayBubbleView* tray_bubble_view_;

    // The aura::Env where this EventHandler is installed. Needed because
    // SingleProcessMash has more than one aura::Env. Cached so this object
    // can unregister itself during TrayBubbleView teardown.
    aura::Env* aura_env_;

    DISALLOW_COPY_AND_ASSIGN(RerouteEventHandler);
  };

  void CloseBubbleView();

  // Focus the default item if no item is focused.
  void FocusDefaultIfNeeded();

  InitParams params_;
  views::BoxLayout* layout_;
  Delegate* delegate_;
  int preferred_width_;
  // |bubble_border_| and |owned_bubble_border_| point to the same thing, but
  // the latter ensures we don't leak it before passing off ownership.
  views::BubbleBorder* bubble_border_;
  std::unique_ptr<views::BubbleBorder> owned_bubble_border_;
  std::unique_ptr<ui::LayerOwner> bubble_content_mask_;
  bool is_gesture_dragging_;

  // True once the mouse cursor was actively moved by the user over the bubble.
  // Only then the OnMouseExitedView() event will get passed on to listeners.
  bool mouse_actively_entered_;

  // Used to find any mouse movements.
  std::unique_ptr<views::MouseWatcher> mouse_watcher_;

  // Used to activate tray bubble view if user tries to interact the tray with
  // keyboard.
  std::unique_ptr<EventHandler> reroute_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(TrayBubbleView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_BUBBLE_VIEW_H_
