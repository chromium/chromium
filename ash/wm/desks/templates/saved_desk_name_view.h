// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_NAME_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_NAME_VIEW_H_

#include <string>

#include "ash/wm/desks/desks_textfield.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// Defines a textfield styled to normally look like a label. Allows modifying
// the name of its corresponding saved desk.
class SavedDeskNameView : public DesksTextfield {
 public:
  METADATA_HEADER(SavedDeskNameView);

  SavedDeskNameView();
  SavedDeskNameView(const SavedDeskNameView&) = delete;
  SavedDeskNameView& operator=(const SavedDeskNameView&) = delete;
  ~SavedDeskNameView() override;

  static constexpr int kSavedDeskNameViewHeight = 20;

  // Commits an on-going name change (if any) by bluring the focus away from any
  // view on `widget`, where `widget` should be the saved desk library widget.
  static void CommitChanges(views::Widget* widget);

  const absl::optional<std::u16string> temporary_name() const {
    return temporary_name_;
  }

  void SetViewName(const std::u16string& name);
  void SetTemporaryName(const std::u16string& new_name) {
    temporary_name_ = new_name;
  }
  void ResetTemporaryName() { temporary_name_.reset(); }

  // Called when the contents in the textfield change. Updates the preferred
  // size of `this`, which invalidates the layout.
  void OnContentsChanged();

  // DesksTextfield:
  gfx::Size CalculatePreferredSize() const override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  // Gets the available width for `this`. It is the largest width `this` can be
  // based on the parent's and visible sibling's preferred sizes. Will always
  // return a value greater than or equal to one.
  int GetAvailableWidth() const;

  // Store the modified text view name if name nudge is removed.
  absl::optional<std::u16string> temporary_name_;
};

BEGIN_VIEW_BUILDER(/* no export */, SavedDeskNameView, DesksTextfield)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::SavedDeskNameView)

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_NAME_VIEW_H_
