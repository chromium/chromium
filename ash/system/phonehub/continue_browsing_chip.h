// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_CONTINUE_BROWSING_CHIP_H_
#define ASH_SYSTEM_PHONEHUB_CONTINUE_BROWSING_CHIP_H_

#include "ash/ash_export.h"
#include "chromeos/components/phonehub/browser_tabs_model.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/button.h"

namespace ash {

// A chip containing a web page info (title, web URL, etc.) that users left off
// from their phone.
class ASH_EXPORT ContinueBrowsingChip : public views::Button,
                                        public views::ButtonListener {
 public:
  explicit ContinueBrowsingChip(
      const chromeos::phonehub::BrowserTabsModel::BrowserTabMetadata& metadata);

  ~ContinueBrowsingChip() override;
  ContinueBrowsingChip(ContinueBrowsingChip&) = delete;
  ContinueBrowsingChip operator=(ContinueBrowsingChip&) = delete;

  // views::ButtonListener:
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;
  const char* GetClassName() const override;

 private:
  GURL url_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_CONTINUE_BROWSING_CHIP_H_
