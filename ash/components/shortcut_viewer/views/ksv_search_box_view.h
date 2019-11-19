// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_SHORTCUT_VIEWER_VIEWS_KSV_SEARCH_BOX_VIEW_H_
#define ASH_COMPONENTS_SHORTCUT_VIEWER_VIEWS_KSV_SEARCH_BOX_VIEW_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/chromeos/search_box/search_box_view_base.h"

namespace search_box {
class SearchBoxViewDelegate;
}  // namespace search_box

namespace keyboard_shortcut_viewer {

// A search_box_SearchBoxViewBase implementation for KeybaordShortcutViewer.
class KSVSearchBoxView : public search_box::SearchBoxViewBase {
 public:
  explicit KSVSearchBoxView(search_box::SearchBoxViewDelegate* delegate);
  ~KSVSearchBoxView() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  void SetAccessibleValue(const base::string16& value);

 private:
  // search_box::SearchBoxViewBase:
  void UpdateBackgroundColor(SkColor color) override;
  void UpdateSearchBoxBorder() override;
  void SetupCloseButton() override;
  void SetupBackButton() override;

  // Accessibility data value. Used to pronounce the number of search results.
  base::string16 accessible_value_;

  DISALLOW_COPY_AND_ASSIGN(KSVSearchBoxView);
};

}  // namespace keyboard_shortcut_viewer

#endif  // ASH_COMPONENTS_SHORTCUT_VIEWER_VIEWS_KSV_SEARCH_BOX_VIEW_H_
