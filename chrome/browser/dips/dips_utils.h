// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_UTILS_H_
#define CHROME_BROWSER_DIPS_DIPS_UTILS_H_

#include <ostream>

#include "base/strings/string_piece_forward.h"

/* Redirect Type Methods */
enum class DIPSRedirectType { kClient, kServer };

base::StringPiece GetHistogramPiece(DIPSRedirectType type);

const char* DIPSRedirectTypeToString(DIPSRedirectType type);

std::ostream& operator<<(std::ostream& os, DIPSRedirectType type);

#endif  // CHROME_BROWSER_DIPS_DIPS_UTILS_H_
