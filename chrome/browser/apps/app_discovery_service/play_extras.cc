// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/play_extras.h"

namespace apps {

PlayExtras::PlayExtras(const bool previously_installed)
    : previously_installed_(previously_installed) {}

bool PlayExtras::GetPreviouslyInstalled() const {
  return previously_installed_;
}

PlayExtras* PlayExtras::AsPlayExtras() {
  return this;
}

}  // namespace apps
