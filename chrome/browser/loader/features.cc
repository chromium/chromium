// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/features.h"

namespace features {

// Keeps fetch keepalive / fetchLater requests alive after last-window close.
BASE_FEATURE(kKeepAliveBrowserProcessAlive, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
