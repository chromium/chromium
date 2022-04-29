// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_COOKIE_ACCESS_TYPE_H_
#define CHROME_BROWSER_DIPS_COOKIE_ACCESS_TYPE_H_

#include "base/strings/string_piece_forward.h"

enum class CookieAccessType { kNone, kRead, kWrite, kReadWrite };

base::StringPiece CookieAccessTypeToString(CookieAccessType type);

#endif  // CHROME_BROWSER_DIPS_COOKIE_ACCESS_TYPE_H_
