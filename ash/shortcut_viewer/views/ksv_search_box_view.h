// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHORTCUT_VIEWER_VIEWS_KSV_SEARCH_BOX_VIEW_H_
#define ASH_SHORTCUT_VIEWER_VIEWS_KSV_SEARCH_BOX_VIEW_H_

#include <string>

#include "ash/search_box/search_box_view_base.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {
class SearchBoxViewDelegate;
}  // namespace ash

namespace keyboard_shortcut_viewer {

// A search_box_SearchBoxViewBase implementation for KeyboardShortcutViewer.
class KSVSearchBoxView : public ash::SearchBoxViewBase {
 public:
  explicit KSVSearchBoxView(ash::SearchBoxViewDelegate* delegate);

  KSVSearchBoxView(const KSVSearchBoxView&) = delete;
  KSVSearchBoxView& operator=(const KSVSearchBoxView&) = delete;

  ~KSVSearchBoxView() override = default;

  // Initializes the search box view style.
  void Initialize();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnThemeChanged() override;

  void SetAccessibleValue(const std::u16string& value);

  // SearchBoxViewBase:
  void UpdateSearchBoxBorder() override;
  void SetupBackButton() override;
  void UpdatePlaceholderTextStyle() override;

 private:
  void SetPlaceholderTextAttributes();

  // Callback for press on the search box close button.
  void CloseButtonPressed();

  SkColor GetBackgroundColor();
  SkColor GetBackButtonColor();
  SkColor GetBorderColor();
  SkColor GetCloseButtonColor();
  SkColor GetPlaceholderTextColor();
  SkColor GetPrimaryIconColor();
  SkColor GetPrimaryTextColor();

  bool ShouldUseFocusedColors();
  bool ShouldUseDarkThemeColors();

  // Accessibility data value. Used to pronounce the number of search results.
  std::u16string accessible_value_;
};

}  // namespace keyboard_shortcut_viewer

#endif  // ASH_SHORTCUT_VIEWER_VIEWS_KSV_SEARCH_BOX_VIEW_H_
