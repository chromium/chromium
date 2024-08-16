// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_ITEM_VIEW_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
class MenuItemView;
}  // namespace views

namespace ash {
class ClipboardHistory;
class ClipboardHistoryItem;

// The base class for menu items of the clipboard history menu.
class ASH_EXPORT ClipboardHistoryItemView : public views::View {
  METADATA_HEADER(ClipboardHistoryItemView, views::View)

 public:
  static std::unique_ptr<ClipboardHistoryItemView>
  CreateFromClipboardHistoryItem(const base::UnguessableToken& item_id,
                                 const ClipboardHistory* clipboard_history,
                                 views::MenuItemView* container);

  ClipboardHistoryItemView(const ClipboardHistoryItemView& rhs) = delete;
  ClipboardHistoryItemView& operator=(const ClipboardHistoryItemView& rhs) =
      delete;
  ~ClipboardHistoryItemView() override;

  // Advances the pseudo focus (backward if reverse is true). Returns whether
  // the view still keeps the pseudo focus.
  bool AdvancePseudoFocus(bool reverse);

  void HandleDeleteButtonPressEvent(const ui::Event& event);

  void HandleMainButtonPressEvent(const ui::Event& event);

  // Makes the Ctrl+V label located underneath this item's contents visible.
  // Will have no effect if called before `Init()`.
  void ShowCtrlVLabel();

  // Attempts to handle the gesture event redirected from `main_button_`.
  void MaybeHandleGestureEventFromMainButton(ui::GestureEvent* event);

  // Called when the selection state has changed.
  void OnSelectionChanged();

  // Returns whether the item's main button has pseudo focus, meaning the item's
  // contents will be pasted if the user presses Enter. An item's background is
  // highlighted when its main button has pseudo focus.
  bool IsMainButtonPseudoFocused() const;

  // Returns whether the item's delete button has pseudo focus, meaning the item
  // will be removed from clipboard history if the user presses Enter. An item's
  // background is not highlighted when its delete button has pseudo focus.
  bool IsDeleteButtonPseudoFocused() const;

  // Called when the mouse click on descendants (such as the main button or
  // the delete button) gets canceled.
  void OnMouseClickOnDescendantCanceled();

  clipboard_history_util::Action action() const { return action_; }

 protected:
  // Used by subclasses to draw contents, such as text or bitmaps. When the
  // clipboard history refresh is enabled, a `ContentsView` observes its sibling
  // `ClipboardHistoryDeleteButton` so that it knows when to clip its contents.
  class ContentsView : public views::View, public views::ViewObserver {
    METADATA_HEADER(ContentsView, views::View)

   public:
    ContentsView();
    ContentsView(const ContentsView& rhs) = delete;
    ContentsView& operator=(const ContentsView& rhs) = delete;
    ~ContentsView() override;

   protected:
    // Returns the region to which this view's contents should be constrained.
    virtual SkPath GetClipPath() = 0;

    bool is_delete_button_visible() { return is_delete_button_visible_; }

   private:
    // views::ViewObserver:
    void OnViewVisibilityChanged(views::View* observed_view,
                                 views::View* starting_view) override;

    // Determines whether the contents need to be clipped to avoid overlapping
    // with the delete button.
    bool is_delete_button_visible_ = false;
  };

  ClipboardHistoryItemView(const base::UnguessableToken& item_id,
                           const ClipboardHistory* clipboard_history,
                           views::MenuItemView* container);

  // Creates the contents view.
  virtual std::unique_ptr<ContentsView> CreateContentsView() = 0;

  const ClipboardHistoryItem* GetClipboardHistoryItem() const;

 private:
  // Indicates the child under pseudo focus, i.e. the view responding to the
  // user actions on the menu item (like clicking the mouse or triggering an
  // accelerator). Note that the child under pseudo focus does not have view
  // focus. It is where "pseudo" comes from.
  // The enumeration types are arranged in the forward focus traversal order.
  enum PseudoFocus {
    // No child is under pseudo focus.
    kEmpty = 0,

    // The main button has pseudo focus.
    kMainButton = 1,

    // The delete button has pseudo focus.
    kDeleteButton = 2,

    // Marks the end. It should not be assigned to `pseudo_focus_`.
    kMaxValue = 3
  };

  class DisplayView;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // Initializes the menu item after its construction.
  void Init();

  // Activates the menu item with the specified action and event flags.
  void Activate(clipboard_history_util::Action action, int event_flags);

  // Calculates the action type when `main_button_` is clicked.
  clipboard_history_util::Action CalculateActionForMainButtonClick() const;

  // Creates the delete button and any necessary containers for its formatting.
  // Sets `delete_button_` in the process.
  std::unique_ptr<views::View> CreateDeleteButton();

  bool ShouldShowDeleteButton() const;

  // Called when receiving pseudo focus for the first time.
  void InitiatePseudoFocus(bool reverse);

  // Updates `pseudo_focus_` and children visibility.
  void SetPseudoFocus(PseudoFocus new_pseudo_focus);

  // Updates the `kSelected` attribute for the current view based on current
  // selection update.
  void UpdateAccessiblitySelectionAttribute();

  // Unique identifier for the `ClipboardHistoryItem` this view represents.
  const base::UnguessableToken item_id_;

  // Owned by `ClipboardHistoryControllerImpl`.
  const raw_ptr<const ClipboardHistory> clipboard_history_;

  const raw_ptr<views::MenuItemView> container_;

  // Owned by the view hierarchy.
  raw_ptr<views::View> main_button_ = nullptr;
  raw_ptr<views::View> ctrl_v_label_ = nullptr;
  raw_ptr<views::View> delete_button_ = nullptr;

  PseudoFocus pseudo_focus_ = PseudoFocus::kEmpty;

  // Indicates whether the menu item is under the gesture long press.
  bool under_gesture_long_press_ = false;

  // Indicates the action to take. It is set when the menu item is activated
  // through `main_button_` or the delete button.
  clipboard_history_util::Action action_ =
      clipboard_history_util::Action::kEmpty;

  base::CallbackListSubscription subscription_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_ITEM_VIEW_H_
