// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_ICUBRIDGE_FEATURES_H_
#define BASE_I18N_ICUBRIDGE_FEATURES_H_

#include "base/feature_list.h"
#include "base/i18n/base_i18n_export.h"

namespace base::i18n {

// Feature to enable using ICU4X normalizer. It is turned off by default.
BASE_I18N_EXPORT BASE_DECLARE_FEATURE(kUseIcu4xNormalizer);

}  // namespace base::i18n

#endif  // BASE_I18N_ICUBRIDGE_FEATURES_H_
