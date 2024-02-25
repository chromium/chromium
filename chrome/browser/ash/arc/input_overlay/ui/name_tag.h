// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_NAME_TAG_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_NAME_TAG_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace arc::input_overlay {

// Create name tag with title and sub-title.
//
// For EditingList (`for_editing_list`=true):
// +----------------+
// |icon |Title|    |
// |     |Sub-title||
// +----------------+
//
// For ButtonOptionsMenu (`for_editing_list`=false):
// +----------------+
// ||Title|         |
// |icon |Sub-title||
// +----------------+
class NameTag : public views::View {
  METADATA_HEADER(NameTag, views::View)

 public:
  static std::unique_ptr<NameTag> CreateNameTag(const std::u16string& title,
                                                bool for_editing_list);

  explicit NameTag(bool for_editing_list);
  NameTag(const NameTag&) = delete;
  NameTag& operator=(const NameTag&) = delete;
  ~NameTag() override;

  void SetTitle(const std::u16string& title);
  void SetAvailableWidth(size_t available_width);

  // Set state depending on `is_error`. If `is_error` true, `error_tooltip` is
  // tooltip text for `error_icon_`.
  void SetState(bool is_error, const std::u16string& error_tooltip);

  views::ImageView* error_icon() const { return error_icon_; }
  views::Label* title_label() { return title_label_; }

 private:
  friend class EditLabelTest;
  friend class OverlayViewTestBase;

  void Init();
  // Child labels are multi-lines. It needs to set fit width depending on
  // whether `error_icon_` shows up.
  void UpdateLabelsFitWidth();

  // True if this view is in EditingList. Otherwise, it is in ButtonOptionsMenu.
  const bool for_editing_list_;

  raw_ptr<views::ImageView> error_icon_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Label> subtitle_label_ = nullptr;

  size_t available_width_ = 0;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_NAME_TAG_H_
