// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_PANEL_VIEW_H_
#define ASH_SYSTEM_MAHI_MAHI_PANEL_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ui {
class Event;
}  // namespace ui

namespace views {
class Label;
}  // namespace views

namespace ash {

class IconButton;

// The code for Mahi main panel view. This view is placed within
// `MahiPanelWidget`.
class ASH_EXPORT MahiPanelView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(MahiPanelView);
  MahiPanelView();
  MahiPanelView(const MahiPanelView&) = delete;
  MahiPanelView& operator=(const MahiPanelView&) = delete;
  ~MahiPanelView() override;

  views::Label* summary_label_for_test() { return summary_label_; }
  IconButton* thumbs_up_button() { return thumbs_up_button_; }
  IconButton* thumbs_down_button() { return thumbs_down_button_; }

 private:
  // Callbacks for thumbs up/thumbs down button.
  void OnThumbsUpButtonPressed(const ui::Event& event);
  void OnThumbsDownButtonPressed(const ui::Event& event);

  // Owned by the views hierarchy.
  raw_ptr<views::Label> summary_label_ = nullptr;
  raw_ptr<IconButton> thumbs_up_button_ = nullptr;
  raw_ptr<IconButton> thumbs_down_button_ = nullptr;

  base::WeakPtrFactory<MahiPanelView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_MAHI_PANEL_VIEW_H_
