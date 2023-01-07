// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHORTCUT_VIEWER_VIEWS_KSV_SEARCH_BOX_VIEW_H_
#define ASH_SHORTCUT_VIEWER_VIEWS_KSV_SEARCH_BOX_VIEW_H_

#include <string>

#include "ash/search_box/search_box_view_base.h"
#include "third_party/skia/include/core/SkColor.h"

namespace keyboard_shortcut_viewer {

// A search_box_SearchBoxViewBase implementation for KeyboardShortcutViewer.
class KSVSearchBoxView : public ash::SearchBoxViewBase {
 public:
  using QueryHandler =
      base::RepeatingCallback<void(const std::u16string& query)>;
  explicit KSVSearchBoxView(QueryHandler query_handler);

  KSVSearchBoxView(const KSVSearchBoxView&) = delete;
  KSVSearchBoxView& operator=(const KSVSearchBoxView&) = delete;

  ~KSVSearchBoxView() override;

  // Initializes the search box view style.
  void Initialize();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnThemeChanged() override;

  void SetAccessibleValue(const std::u16string& value);

  // SearchBoxViewBase:
  void HandleQueryChange(const std::u16string& query,
                         bool initiated_by_user) override;
  void UpdateSearchBoxBorder() override;
  void UpdatePlaceholderTextStyle() override;
  void OnSearchBoxActiveChanged(bool active) override;

 private:
  void SetPlaceholderTextAttributes();

  // Callback for press on the search box close button.
  void CloseButtonPressed();

  SkColor GetBackgroundColor();
  SkColor GetBorderColor();
  SkColor GetCloseButtonColor();
  SkColor GetPlaceholderTextColor();
  SkColor GetPrimaryIconColor();
  SkColor GetPrimaryTextColor();

  bool ShouldUseFocusedColors();
  bool ShouldUseDarkThemeColors();

  // Callback that gets invoked to handle search box query changes.
  const QueryHandler query_handler_;

  // Accessibility data value. Used to pronounce the number of search results.
  std::u16string accessible_value_;
};

}  // namespace keyboard_shortcut_viewer

#endif  // ASH_SHORTCUT_VIEWER_VIEWS_KSV_SEARCH_BOX_VIEW_H_
