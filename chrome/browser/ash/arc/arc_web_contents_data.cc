// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/arc_web_contents_data.h"

namespace arc {

// static
const char ArcWebContentsData::kArcTransitionFlag[] = "ArcTransition";

WEB_CONTENTS_USER_DATA_KEY_IMPL(ArcWebContentsData)

}  // namespace arc
