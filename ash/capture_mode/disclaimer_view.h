// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_DISCLAIMER_VIEW_H_
#define ASH_CAPTURE_MODE_DISCLAIMER_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/system_panel_view.h"
#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

// IDs for certain views that are used for UI testing.
enum DisclaimerViewId {
  kDisclaimerViewAcceptButtonId = 1,
  kDisclaimerViewDeclineButtonId,
  kDisclaimerViewParagraphOneId,
  kDisclaimerViewParagraphTwoId,
  kDisclaimerViewParagraphThreeId,
};

// DisclaimerView shows the consent disclaimer for the Scanner and Sunfish
// features. It shows an image, some disclaimer text, and two buttons (accept
// and decline).
// If `is_reminder` is true, the decline button will be removed, and the title
// and accept button strings will be adjusted.
class ASH_EXPORT DisclaimerView : public views::View {
  METADATA_HEADER(DisclaimerView, views::View)

 public:
  DisclaimerView(bool is_reminder,
                 base::RepeatingClosure press_accept_button_callback,
                 base::RepeatingClosure press_decline_button_callback,
                 base::RepeatingClosure press_terms_of_service_callback,
                 base::RepeatingClosure press_learn_more_link_callback);
  DisclaimerView(const DisclaimerView&) = delete;
  DisclaimerView& operator=(const DisclaimerView&) = delete;
  ~DisclaimerView() override;

  views::MdTextButton* accept_button() { return accept_button_; }

  static std::unique_ptr<views::Widget> CreateWidget(
      aura::Window* const root,
      bool is_reminder,
      base::RepeatingClosure press_accept_button_callback,
      base::RepeatingClosure press_decline_button_callback,
      base::RepeatingClosure press_terms_of_service_callback,
      base::RepeatingClosure press_learn_more_link_callback);

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  raw_ptr<views::MdTextButton> accept_button_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_DISCLAIMER_VIEW_H_
