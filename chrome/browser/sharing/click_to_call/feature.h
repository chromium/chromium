// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_FEATURE_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_FEATURE_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"

#if defined(OS_ANDROID)
// Feature to allow devices to receive the click to call message.
extern const base::Feature kClickToCallReceiver;
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_CLICK_TO_CALL)
// Feature to allow click to call gets processed on desktop.
extern const base::Feature kClickToCallUI;

// Feature to show click to call in context menu when selected text is a phone
// number.
extern const base::Feature kClickToCallContextMenuForSelectedText;

// Feature to use the second version of the phone number detection.
extern const base::Feature kClickToCallDetectionV2;
#endif  // BUILDFLAG(ENABLE_CLICK_TO_CALL)

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_FEATURE_H_
