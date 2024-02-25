// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"

#include <algorithm>

namespace ash::quick_start {

FidoAssertionInfo::FidoAssertionInfo() = default;

FidoAssertionInfo::~FidoAssertionInfo() = default;

FidoAssertionInfo::FidoAssertionInfo(const FidoAssertionInfo& other) = default;

FidoAssertionInfo& FidoAssertionInfo::operator=(
    const FidoAssertionInfo& other) = default;

bool FidoAssertionInfo::operator==(const FidoAssertionInfo& rhs) const =
    default;

}  // namespace ash::quick_start
