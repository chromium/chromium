// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MALL_MALL_FEATURES_H_
#define CHROME_BROWSER_ASH_MALL_MALL_FEATURES_H_

#include "base/feature_list.h"

namespace ash {

// Feature which causes launches of the Mall app to include device context
// information.
BASE_DECLARE_FEATURE(kCrosMallEnableContext);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MALL_MALL_FEATURES_H_
