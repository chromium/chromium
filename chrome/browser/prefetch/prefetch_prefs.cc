// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_prefs.h"

namespace prefetch {

PreloadPagesState GetPreloadPagesState(const PrefService& prefs) {
  // TODO(crbug.com/1263586): implement.
  return PreloadPagesState::STANDARD_PRELOADING;
}

void SetPreloadPagesState(PrefService* prefs, PreloadPagesState state) {
  // TODO(crbug.com/1263586): implement.
}

}  // namespace prefetch
