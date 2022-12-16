// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_UMA_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_UMA_H_

namespace arc::input_overlay {

// These values are about how the reposition is achieved.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RepositionType {
  kTouchscreenDragRepostion = 0,
  kMouseDragRepostion = 1,
  kKeyboardArrowKeyReposition = 2,
  kMaxValue = kKeyboardArrowKeyReposition
};

void RecordInputOverlayFeatureState(bool enable);

void RecordInputOverlayMappingHintState(bool enable);

void RecordInputOverlayCustomizedUsage();

// Record when finishing action dragging or releasing arrow key.
void RecordInputOverlayActionReposition(RepositionType type);

// Record when finishing menu entry dragging or releasing arrow key.
void RecordInputOverlayMenuEntryReposition(RepositionType type);

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_UMA_H_
