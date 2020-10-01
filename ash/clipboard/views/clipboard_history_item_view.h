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

  // Returns whether the menu item is under selection.
  bool IsSelected() const;

  // Called when the selection state has changed.
  void OnSelectionChanged();

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

    // Called when the `container_`'s selection state has changed.
    void OnSelectionChanged();

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
  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // Executes |command_id| on the delegate.
  void ExecuteCommand(int command_id, const ui::Event& event);

  // Owned by ClipboardHistoryMenuModelAdapter.
  const ClipboardHistoryItem* const clipboard_history_item_;

  views::MenuItemView* const container_;

  ContentsView* contents_view_ = nullptr;

  MainButton* main_button_ = nullptr;

  views::PropertyChangedSubscription subscription_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_ITEM_VIEW_H_
