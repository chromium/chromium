// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_COMBOBOX_H_
#define ASH_STYLE_COMBOBOX_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/combobox_model_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ui {
class ComboboxModel;
}

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

// A stylized non-editable combobox driven by `ui::ComboboxModel`.
class ASH_EXPORT Combobox : public views::Button,
                            public ui::ComboboxModelObserver,
                            public views::WidgetObserver {
  METADATA_HEADER(Combobox, views::Button)

 public:
  static constexpr gfx::Insets kComboboxBorderInsets =
      gfx::Insets::TLBR(4, 10, 4, 4);

  // `model` is owned by the combobox when using this constructor.
  explicit Combobox(std::unique_ptr<ui::ComboboxModel> model);
  // `model` is not owned by the combobox when using this constructor.
  explicit Combobox(ui::ComboboxModel* model);
  Combobox(const Combobox&) = delete;
  Combobox& operator=(const Combobox&) = delete;
  ~Combobox() override;

  // Sets the callback that is invoked when the selected item changes. Note that
  // this works same as `views::Combobox::SetCallback`.
  void SetSelectionChangedCallback(base::RepeatingClosure callback);

  // Gets/Sets the selected index.
  std::optional<size_t> GetSelectedIndex() const { return selected_index_; }
  void SetSelectedIndex(std::optional<size_t> index);

  // Looks for the first occurrence of `value` in `model_`. If found, selects
  // the found index and returns true. Otherwise simply noops and returns false.
  bool SelectValue(const std::u16string& value);

  // Returns whether or not the menu is currently running.
  bool IsMenuRunning() const;
  gfx::Size GetMenuViewSize() const;

  views::View* MenuItemAtIndex(int index) const;
  views::View* MenuView() const;

  // views::Button:
  void SetCallback(PressedCallback callback) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnBlur() override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void Layout(PassKey) override;

  // WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& bounds) override;

  std::u16string GetTextForRow(size_t row) const;

  // Test method exposing MenuSelectionAt.
  void SelectMenuItemForTest(size_t index);

 private:
  friend class ComboboxTest;
  class ComboboxMenuView;
  class ComboboxEventHandler;

  // Gets expected menu bounds according to combox location.
  gfx::Rect GetExpectedMenuBounds() const;

  // Called when there has been a selection from the menu.
  void MenuSelectionAt(size_t index);

  // Called when the combobox is pressed.
  void OnComboboxPressed();

  // Shows/Closes the drop down menu.
  void ShowDropDownMenu();
  void CloseDropDownMenu();

  // Called when a selection is made.
  void OnPerformAction();

  // Overridden from ComboboxModelObserver:
  void OnComboboxModelChanged(ui::ComboboxModel* model) override;
  void OnComboboxModelDestroying(ui::ComboboxModel* model) override;

  // views::Button:
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) override;
  bool OnKeyPressed(const ui::KeyEvent& e) override;
  void OnEnabledChanged() override;

  void UpdateExpandedCollapsedAccessibleState() const;
  void UpdateAccessibleAccessibleActiveDescendantId();
  void UpdateAccessibleDefaultAction();

  // Optionally used to tie the lifetime of the model to this combobox. See
  // constructor.
  std::unique_ptr<ui::ComboboxModel> owned_model_;

  // Reference to our model, which may be owned or not.
  raw_ptr<ui::ComboboxModel> model_;

  const raw_ptr<views::Label> title_ = nullptr;
  const raw_ptr<views::ImageView> drop_down_arrow_ = nullptr;

  // Callback notified when the selected index changes.
  base::RepeatingClosure callback_;

  // The current selected index; nullopt means no selection.
  std::optional<size_t> selected_index_ = std::nullopt;

  // The selection that committed by performing selection changed action.
  std::optional<size_t> last_commit_selection_ = std::nullopt;

  // A handler handles mouse and touch event happening outside combobox and drop
  // down menu. This is mainly used to decide if we should close the drop down
  // menu.
  std::unique_ptr<ComboboxEventHandler> event_handler_;

  // Drop down menu view owned by menu widget.
  raw_ptr<ComboboxMenuView> menu_view_ = nullptr;

  // Drop down menu widget.
  views::UniqueWidgetPtr menu_;

  // Like MenuButton, we use a time object in order to keep track of when the
  // combobox was closed. The time is used for simulating menu behavior; that
  // is, if the menu is shown and the button is pressed, we need to close the
  // menu. There is no clean way to get the second click event because the
  // menu is displayed using a modal loop and, unlike regular menus in Windows,
  // the button is not part of the displayed menu.
  base::TimeTicks closed_time_;

  base::ScopedObservation<ui::ComboboxModel, ui::ComboboxModelObserver>
      observation_{this};

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observer_{this};

  base::WeakPtrFactory<Combobox> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_STYLE_COMBOBOX_H_
