// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_features.h"

namespace enterprise_data_protection {

BASE_FEATURE(kEnableSinglePageAppDataProtection,
             "EnableSinglePageAppDataProtection",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace enterprise_data_protection
