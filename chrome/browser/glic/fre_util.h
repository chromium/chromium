// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_FRE_UTIL_H_
#define CHROME_BROWSER_GLIC_FRE_UTIL_H_

#include <string>

class GURL;

namespace glic {

GURL GetFreURL();
std::string GetHotkeyString();

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_FRE_UTIL_H_
