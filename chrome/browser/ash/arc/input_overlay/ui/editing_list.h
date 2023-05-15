// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDITING_LIST_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDITING_LIST_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

class DisplayOverlayController;

// EditingList contains the list of controls.
//    _________________________________
//   |icon        "Editing"        icon|
//   |   ___________________________   |
//   |  |                           |  |
//   |  |    zero-state or          |  |
//   |  |    scrollable list        |  |
//   |  |___________________________|  |
//   |_________________________________|
//
class EditingList : public views::View {
 public:
  static EditingList* Show(DisplayOverlayController* controller);

  explicit EditingList(DisplayOverlayController* display_overlay_controller);
  EditingList(const EditingList&) = delete;
  EditingList& operator=(const EditingList&) = delete;
  ~EditingList() override;

 private:
  void Init();
  bool HasControls() const;

  // Add UI components to |container| as children.
  void AddHeader(views::View* container);
  void AddZeroStateContent(views::View* container);
  void AddControlListContent(views::View* container);

  // Functions related to buttons.
  void OnAddButtonPressed();
  void OnDoneButtonPressed();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  raw_ptr<DisplayOverlayController> controller_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDITING_LIST_H_
