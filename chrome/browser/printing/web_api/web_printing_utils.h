// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_UTILS_H_
#define CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_UTILS_H_

#include <string_view>

namespace printing {

struct AdvancedCapability;
struct PrinterSemanticCapsAndDefaults;

namespace internal {

const AdvancedCapability* FindAdvancedCapability(
    const PrinterSemanticCapsAndDefaults& caps,
    std::string_view capability_name);

}  // namespace internal
}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_UTILS_H_
