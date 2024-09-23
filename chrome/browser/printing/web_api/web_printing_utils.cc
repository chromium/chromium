// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/web_api/web_printing_utils.h"

#include "base/ranges/algorithm.h"
#include "printing/backend/print_backend.h"

namespace printing::internal {

const AdvancedCapability* FindAdvancedCapability(
    const PrinterSemanticCapsAndDefaults& caps,
    std::string_view capability_name) {
  auto itr = base::ranges::find(caps.advanced_capabilities, capability_name,
                                &AdvancedCapability::name);
  if (itr != caps.advanced_capabilities.end()) {
    return &*itr;
  }
  return nullptr;
}

}  // namespace printing::internal
