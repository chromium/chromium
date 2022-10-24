// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_constants.h"

namespace ash {

static_assert(kTrayMenuWidth == kUnifiedFeaturePodHorizontalSidePadding * 2 +
                                    kUnifiedFeaturePodHorizontalMiddlePadding *
                                        (kUnifiedFeaturePodItemsInRow - 1) +
                                    kUnifiedFeaturePodSize.width() *
                                        kUnifiedFeaturePodItemsInRow,
              "Total feature pod width does not match kTrayMenuWidth");

}  // namespace ash
