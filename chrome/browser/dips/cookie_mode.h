// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_COOKIE_MODE_H_
#define CHROME_BROWSER_DIPS_COOKIE_MODE_H_

#include <ostream>

#include "base/strings/string_piece_forward.h"

enum class DIPSCookieMode {
  kStandard,
  kOffTheRecord,
  kBlock3PC,  // block third-party cookies
  kOffTheRecord_Block3PC
};

DIPSCookieMode GetDIPSCookieMode(bool is_otr, bool block_third_party_cookies);

base::StringPiece GetHistogramSuffix(DIPSCookieMode mode);

const char* DIPSCookieModeToString(DIPSCookieMode mode);

std::ostream& operator<<(std::ostream& os, DIPSCookieMode mode);

#endif  // CHROME_BROWSER_DIPS_COOKIE_MODE_H_
