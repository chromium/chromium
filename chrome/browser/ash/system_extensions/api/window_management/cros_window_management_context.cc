// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/api/window_management/cros_window_management_context.h"

#include "chrome/browser/ash/system_extensions/api/window_management/cros_window_management_context_factory.h"

namespace ash {

// static
CrosWindowManagementContext& CrosWindowManagementContext::Get(
    Profile* profile) {
  return *CrosWindowManagementContextFactory::GetForProfileIfExists(profile);
}

CrosWindowManagementContext::CrosWindowManagementContext() = default;

CrosWindowManagementContext::~CrosWindowManagementContext() = default;

}  // namespace ash
