// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_TITLE_VIEW_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_TITLE_VIEW_H_

#include "ash/style/icon_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash::video_conference {

// Part of the vc bubble that holds the icon, title, mic indicator and
// sidetone toggle.
class TitleView : public views::BoxLayoutView {
  METADATA_HEADER(TitleView, views::BoxLayoutView)
 public:
  explicit TitleView();
  TitleView(const TitleView&) = delete;
  TitleView& operator=(const TitleView&) = delete;
  ~TitleView() override;

 private:
  raw_ptr<IconButton> sidetone_button_ = nullptr;
  views::UniqueWidgetPtr sidetone_bubble_widget_;

  void OnSidetoneButtonClicked(const ui::Event& event);
  void CloseSidetoneBubble();
  void ShowSidetoneBubble(const bool supported);
  base::WeakPtrFactory<TitleView> weak_ptr_factory_{this};
};

}  // namespace ash::video_conference

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_TITLE_VIEW_H_
