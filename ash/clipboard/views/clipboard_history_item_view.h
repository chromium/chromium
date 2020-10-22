// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_ITEM_VIEW_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_ITEM_VIEW_H_

#include "ui/views/controls/button/image_button.h"
#include "ui/views/view_targeter_delegate.h"

namespace views {
class MenuItemView;
}  // namespace views

namespace ash {
class ClipboardHistoryItem;
class ClipboardHistoryResourceManager;

// The base class for menu items of the clipboard history menu.
class ClipboardHistoryItemView : public views::View {
 public:
  static std::unique_ptr<ClipboardHistoryItemView>
  CreateFromClipboardHistoryItem(
      const ClipboardHistoryItem& item,
      const ClipboardHistoryResourceManager* resource_manager,
      views::MenuItemView* container);

  ClipboardHistoryItemView(const ClipboardHistoryItemView& rhs) = delete;
  ClipboardHistoryItemView& operator=(const ClipboardHistoryItemView& rhs) =
      delete;
  ~ClipboardHistoryItemView() override;

  // Initializes the menu item.
  void Init();

  // Called when the selection state has changed.
  void OnSelectionChanged();

  // Advances the pseudo focus (backward if reverse is true). Returns whether
  // the view still keeps the pseudo focus.
  bool AdvancePseudoFocus(bool reverse);

  const views::View* delete_button_for_test() const {
    return contents_view_->delete_button();
  }

 protected:
  class MainButton;

  // The button to delete the menu item and its corresponding clipboard data.
  class DeleteButton : public views::ImageButton {
   public:
    explicit DeleteButton(ClipboardHistoryItemView* listener);
    DeleteButton(const DeleteButton& rhs) = delete;
    DeleteButton& operator=(const DeleteButton& rhs) = delete;
    ~DeleteButton() override;

   private:
    // views::ImageButton:
    const char* GetClassName() const override;
  };

  // Used by subclasses to draw contents, such as text or bitmaps.
  class ContentsView : public views::View, public views::ViewTargeterDelegate {
   public:
    explicit ContentsView(ClipboardHistoryItemView* container);
    ContentsView(const ContentsView& rhs) = delete;
    ContentsView& operator=(const ContentsView& rhs) = delete;
    ~ContentsView() override;

    // Install DeleteButton on the contents view.
    void InstallDeleteButton();

    views::View* delete_button() { return delete_button_; }
    const views::View* delete_button() const { return delete_button_; }

   protected:
    virtual DeleteButton* CreateDeleteButton() = 0;

    // The parent of ContentsView.
    ClipboardHistoryItemView* const container_;

   private:
    // views::ViewTargeterDelegate:
    bool DoesIntersectRect(const views::View* target,
                           const gfx::Rect& rect) const override;

    // Owned by the view hierarchy.
    DeleteButton* delete_button_ = nullptr;
  };

  ClipboardHistoryItemView(const ClipboardHistoryItem* clipboard_history_item,
                           views::MenuItemView* container);

  // Records histograms after the button is pressed.
  void RecordButtonPressedHistogram(bool is_delete_button);

  // Creates the contents view.
  virtual std::unique_ptr<ContentsView> CreateContentsView() = 0;

  // Returns the opacity of the menu item view's contents depending on the
  // enabled state.
  float GetContentsOpacity() const;

  const ClipboardHistoryItem* clipboard_history_item() {
    return clipboard_history_item_;
  }

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

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // Executes |command_id| on the delegate.
  void ExecuteCommand(int command_id, const ui::Event& event);

  // Calculates the command id, which indicates the response to user actions.
  int CalculateCommandId() const;

  // Returns whether the highlight background should show.
  bool ShouldHighlight() const;

  bool ShouldShowDeleteButton() const;

  // Called when receiving pseudo focus for the first time.
  void InitiatePseudoFocus(bool reverse);

  // Updates `pseudo_focus_` and children visibility.
  void SetPseudoFocus(PseudoFocus new_pseudo_focus);

  // Owned by ClipboardHistoryMenuModelAdapter.
  const ClipboardHistoryItem* const clipboard_history_item_;

  views::MenuItemView* const container_;

  ContentsView* contents_view_ = nullptr;

  MainButton* main_button_ = nullptr;

  PseudoFocus pseudo_focus_ = PseudoFocus::kEmpty;

  views::PropertyChangedSubscription subscription_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_ITEM_VIEW_H_
