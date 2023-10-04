// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTIVE_USE_UTIL_H_
#define CHROME_BROWSER_ACTIVE_USE_UTIL_H_

#include "build/build_config.h"
#include "chrome/install_static/buildflags.h"

constexpr bool kShouldRecordActiveUse =
    !BUILDFLAG(IS_WIN) || BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION);

#endif  // CHROME_BROWSER_ACTIVE_USE_UTIL_H_
