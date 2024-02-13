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

namespace ash {

// The code for Mahi main panel view. This view is placed within
// `MahiPanelWidget`.
class ASH_EXPORT MahiPanelView : public views::BoxLayoutView {
  METADATA_HEADER(MahiPanelView, views::BoxLayoutView)

 public:
  enum ViewId {
    kCloseButton = 1,
    kSummaryLabel,
    kThumbsUpButton,
    kThumbsDownButton,
    kLearnMoreLink,
  };

  MahiPanelView();
  MahiPanelView(const MahiPanelView&) = delete;
  MahiPanelView& operator=(const MahiPanelView&) = delete;
  ~MahiPanelView() override;

 private:
  // Callbacks for buttons and link.
  void OnThumbsUpButtonPressed(const ui::Event& event);
  void OnThumbsDownButtonPressed(const ui::Event& event);
  void OnCloseButtonPressed(const ui::Event& event);
  void OnLearnMoreLinkClicked();

  base::WeakPtrFactory<MahiPanelView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_MAHI_PANEL_VIEW_H_
