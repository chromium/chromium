// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUNDS_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUNDS_H_

#include <stddef.h>

#include <array>

class GURL;

const size_t kNtpBackgroundsCount = 5;
std::array<GURL, kNtpBackgroundsCount> GetNtpBackgrounds();

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUNDS_H_
