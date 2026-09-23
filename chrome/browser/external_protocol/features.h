// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTERNAL_PROTOCOL_FEATURES_H_
#define CHROME_BROWSER_EXTERNAL_PROTOCOL_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// Blocks web-initiated external protocol launches with a local+ scheme.
// See crbug.com/539667062.
BASE_DECLARE_FEATURE(kLocalOnlyAppProtocolPrefix);

}  // namespace features

#endif  // CHROME_BROWSER_EXTERNAL_PROTOCOL_FEATURES_H_
