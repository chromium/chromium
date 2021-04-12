// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_provider.h"

#include "base/logging.h"
#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider_factory.h"

// static
SystemExtensionsProvider* SystemExtensionsProvider::Get(Profile* profile) {
  return SystemExtensionsProviderFactory::GetForProfileIfExists(profile);
}

SystemExtensionsProvider::SystemExtensionsProvider() {
  install_manager_ = std::make_unique<SystemExtensionsInstallManager>();
}

SystemExtensionsProvider::~SystemExtensionsProvider() = default;
