// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/feature.h"

#if defined(OS_ANDROID)
const base::Feature kClickToCallReceiver{"ClickToCallReceiver",
                                         base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_CLICK_TO_CALL)
const base::Feature kClickToCallContextMenuForSelectedText{
    "ClickToCallContextMenuForSelectedText", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kClickToCallUI{"ClickToCallUI",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kClickToCallDetectionV2{"ClickToCallDetectionV2",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(ENABLE_CLICK_TO_CALL)
