// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_CONTINUE_BROWSING_CHIP_H_
#define ASH_SYSTEM_PHONEHUB_CONTINUE_BROWSING_CHIP_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/browser_tabs_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/button.h"

namespace ash {

namespace phonehub {
class UserActionRecorder;
}

// A chip containing a web page info (title, web URL, etc.) that users left off
// from their phone.
class ASH_EXPORT ContinueBrowsingChip : public views::Button {
  METADATA_HEADER(ContinueBrowsingChip, views::Button)
 public:
  ContinueBrowsingChip(
      const phonehub::BrowserTabsModel::BrowserTabMetadata& metadata,
      int index,
      size_t total_count,
      phonehub::UserActionRecorder* user_action_recorder);

  ~ContinueBrowsingChip() override;
  ContinueBrowsingChip(ContinueBrowsingChip&) = delete;
  ContinueBrowsingChip operator=(ContinueBrowsingChip&) = delete;

  // views::Button:
  void OnPaintBackground(gfx::Canvas* canvas) override;

 private:
  void ButtonPressed();

  // The URL of the tab to open.
  GURL url_;

  // The index of the chip as it is ordered in the parent view.
  int index_;

  // The total number of chips in the parent view.
  size_t total_count_;

  raw_ptr<phonehub::UserActionRecorder> user_action_recorder_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_CONTINUE_BROWSING_CHIP_H_
