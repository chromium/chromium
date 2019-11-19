// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_constants.h"

#include "base/logging.h"

namespace ash {

const int kTrayTextFontSizeIncrease = 2;

// Size of tray items on the primary axis.
const int kTrayItemSize = 32;

const int kTrayMenuWidth = 360;

const int kTrayPopupAutoCloseDelayInSeconds = 2;
const int kTrayPopupPaddingHorizontal = 18;
const int kTrayPopupButtonEndMargin = 10;
const int kTrayPopupLabelHorizontalPadding = 4;
const int kTrayPopupSliderHorizontalPadding = 16;
const int kTrayPopupItemMinHeight = 48;
const int kTrayPopupItemMinStartWidth = 48;
const int kTrayPopupItemMinEndWidth =
    kMenuIconSize + 2 * kTrayPopupButtonEndMargin;

const int kTrayPopupLabelRightPadding = 8;

const int kTrayToggleButtonWidth = 68;

const int kMenuIconSize = 20;
const int kMenuButtonSize = 48;
const int kMenuSeparatorVerticalPadding = 4;
const int kMenuExtraMarginFromLeftEdge = 4;
const int kMenuEdgeEffectivePadding =
    kMenuExtraMarginFromLeftEdge + (kMenuButtonSize - kMenuIconSize) / 2;

const int kTrayPopupInkDropInset = 4;
const int kTrayPopupInkDropCornerRadius = 2;

static_assert(kTrayMenuWidth == kUnifiedFeaturePodHorizontalSidePadding * 2 +
                                    kUnifiedFeaturePodHorizontalMiddlePadding *
                                        (kUnifiedFeaturePodItemsInRow - 1) +
                                    kUnifiedFeaturePodSize.width() *
                                        kUnifiedFeaturePodItemsInRow,
              "Total feature pod width does not match kTrayMenuWidth");

}  // namespace ash
