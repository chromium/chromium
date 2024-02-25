// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_PALETTE_TRAY_TEST_API_H_
#define ASH_SYSTEM_PALETTE_PALETTE_TRAY_TEST_API_H_

#include "ash/system/palette/palette_tray.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class PaletteToolManager;
class PaletteWelcomeBubble;
class TrayBubbleWrapper;

// Use the api in this class to test PaletteTray.
class PaletteTrayTestApi {
 public:
  explicit PaletteTrayTestApi(PaletteTray* palette_tray);

  PaletteTrayTestApi(const PaletteTrayTestApi&) = delete;
  PaletteTrayTestApi& operator=(const PaletteTrayTestApi&) = delete;

  ~PaletteTrayTestApi();

  PaletteToolManager* palette_tool_manager() {
    return palette_tray_->palette_tool_manager_.get();
  }

  PaletteWelcomeBubble* welcome_bubble() {
    return palette_tray_->welcome_bubble_.get();
  }

  TrayBubbleWrapper* tray_bubble_wrapper() {
    return palette_tray_->bubble_.get();
  }

  void OnStylusStateChanged(ui::StylusState state) {
    palette_tray_->OnStylusStateChanged(state);
  }

  // Have the tray act as though it is on a display with a stylus
  void SetDisplayHasStylus() { palette_tray_->SetDisplayHasStylusForTesting(); }

 private:
  raw_ptr<PaletteTray, DanglingUntriaged> palette_tray_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_PALETTE_TRAY_TEST_API_H_
