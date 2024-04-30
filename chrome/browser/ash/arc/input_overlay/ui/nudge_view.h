// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_NUDGE_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_NUDGE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace arc::input_overlay {
// TODO(b/253646354): This should be removed when removing the Beta flag.
// This shows education nudge for AlphaV2 version with a dot indicator.
// ------------------------------
// | --------------------        |
// | | [icon]  (string)  |       |
// | --------------------  [Dot] |
// ------------------------------|
class NudgeView : public views::View {
  METADATA_HEADER(NudgeView, views::View)

 public:
  static NudgeView* Show(views::View* parent, views::View* menu_entry);

  NudgeView(views::View* parent, views::View* menu_entry);
  NudgeView(const NudgeView&) = delete;
  NudgeView& operator=(const NudgeView&) = delete;
  ~NudgeView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  void Init();

  raw_ptr<views::View> parent_;
  // Owned by overlay widget root view.
  raw_ptr<views::View> menu_entry_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_NUDGE_VIEW_H_
