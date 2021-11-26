// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/fake_api_guard_delegate.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

FakeApiGuardDelegate::Factory::Factory(bool is_extension_force_installed)
    : is_extension_force_installed_(is_extension_force_installed) {}

FakeApiGuardDelegate::Factory::~Factory() = default;

std::unique_ptr<ApiGuardDelegate>
FakeApiGuardDelegate::Factory::CreateInstance() {
  return base::WrapUnique<ApiGuardDelegate>(
      new FakeApiGuardDelegate(is_extension_force_installed_));
}

FakeApiGuardDelegate::FakeApiGuardDelegate(bool is_extension_force_installed)
    : is_extension_force_installed_(is_extension_force_installed) {}

FakeApiGuardDelegate::~FakeApiGuardDelegate() = default;

bool FakeApiGuardDelegate::IsExtensionForceInstalled(
    content::BrowserContext* context,
    const std::string& extension_id) {
  return is_extension_force_installed_;
}

}  // namespace chromeos
