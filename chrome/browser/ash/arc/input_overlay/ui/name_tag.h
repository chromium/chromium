// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_NAME_TAG_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_NAME_TAG_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/color/color_id.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace arc::input_overlay {

// Create name tag with title and sub-title as:
// +----------------+
// |icon |Title|    |
// |     |Sub-title||
// +----------------+
class NameTag : public views::View {
 public:
  static std::unique_ptr<NameTag> CreateNameTag(const std::u16string& title);

  NameTag();
  NameTag(const NameTag&) = delete;
  NameTag& operator=(const NameTag&) = delete;
  ~NameTag() override;

  void SetTitle(const std::u16string& title);
  void SetSubtitle(const std::u16string& sub_title);

  // Set state depending on `is_error`. If `is_error` true, `error_tooltip` is
  // tooltip text for `error_icon_`.
  void SetState(bool is_error, const std::u16string& error_tooltip);

  views::ImageView* error_icon() const { return error_icon_; }

 private:
  friend class EditLabelTest;

  void Init();

  void SetTextColor(ui::ColorId color_id);

  raw_ptr<views::ImageView> error_icon_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Label> subtitle_label_ = nullptr;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_NAME_TAG_H_
