// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/api_guard_delegate.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_management.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

// static
ApiGuardDelegate::Factory* ApiGuardDelegate::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<ApiGuardDelegate> ApiGuardDelegate::Factory::Create() {
  if (test_factory_) {
    return test_factory_->CreateInstance();
  }
  return base::WrapUnique<ApiGuardDelegate>(new ApiGuardDelegate());
}

// static
void ApiGuardDelegate::Factory::SetForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

ApiGuardDelegate::Factory::Factory() = default;
ApiGuardDelegate::Factory::~Factory() = default;

ApiGuardDelegate::ApiGuardDelegate() = default;
ApiGuardDelegate::~ApiGuardDelegate() = default;

bool ApiGuardDelegate::IsExtensionForceInstalled(
    content::BrowserContext* context,
    const std::string& extension_id) {
  const auto force_install_list =
      extensions::ExtensionManagementFactory::GetForBrowserContext(context)
          ->GetForceInstallList();
  return force_install_list->FindKey(extension_id) != nullptr;
}

}  // namespace chromeos
