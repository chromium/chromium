// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"

namespace arc {

// static
const char ArcWebContentsData::kArcTransitionFlag[] = "ArcTransition";

ArcWebContentsData::ArcWebContentsData(content::WebContents* web_contents)
    : content::WebContentsUserData<ArcWebContentsData>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ArcWebContentsData);

}  // namespace arc
