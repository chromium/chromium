// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_ACTIVITY_LIST_BUBBLE_GLIC_ACTIVITY_LIST_BUBBLE_ROW_BUTTON_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_ACTIVITY_LIST_BUBBLE_GLIC_ACTIVITY_LIST_BUBBLE_ROW_BUTTON_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_row_view_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace glic {

// Button representing an activity or task entry in the GlicActivityListBubble.
class GlicActivityListBubbleRowButton : public views::Button {
  METADATA_HEADER(GlicActivityListBubbleRowButton, views::Button)

 public:
  explicit GlicActivityListBubbleRowButton(
      const GlicActivityRowViewModel& model);
  GlicActivityListBubbleRowButton(const GlicActivityListBubbleRowButton&) =
      delete;
  GlicActivityListBubbleRowButton& operator=(
      const GlicActivityListBubbleRowButton&) = delete;
  ~GlicActivityListBubbleRowButton() override;

  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  std::u16string_view GetTitleText() const;
  std::u16string_view GetSubtitleText() const;
  views::ImageButton* GetRedirectIconForTesting() { return redirect_icon_; }

 private:
  // Forwards clicks on the redirect icon to the row button.
  void OnRedirectIconPressed(const ui::Event& event);

  // Update the accessible name for the button based on the title and subtitle
  // text.
  void UpdateAccessibleName();

  // Whether the task in this row has an existing tab or not.
  bool has_tab_ = false;

  raw_ptr<views::ImageView> row_icon_ = nullptr;
  raw_ptr<views::ImageButton> redirect_icon_ = nullptr;
  raw_ptr<views::Label> title_;  // Never null.
  raw_ptr<views::Label> subtitle_ = nullptr;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_ACTIVITY_LIST_BUBBLE_GLIC_ACTIVITY_LIST_BUBBLE_ROW_BUTTON_H_
