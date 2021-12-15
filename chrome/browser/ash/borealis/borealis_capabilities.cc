// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_capabilities.h"

#include "third_party/cros_system_api/constants/vm_tools.h"

namespace borealis {

BorealisCapabilities::~BorealisCapabilities() = default;

std::string BorealisCapabilities::GetSecurityContext() const {
  return vm_tools::kConciergeSecurityContext;
}

}  // namespace borealis
