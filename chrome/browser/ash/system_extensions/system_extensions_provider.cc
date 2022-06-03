// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_provider.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider_factory.h"
#include "chrome/browser/ash/system_extensions/system_extensions_web_ui_config_map.h"

constexpr char kSystemExtensionsProfileDirectory[] = "SystemExtensions";

// static
SystemExtensionsProvider* SystemExtensionsProvider::Get(Profile* profile) {
  return SystemExtensionsProviderFactory::GetForProfileIfExists(profile);
}

bool SystemExtensionsProvider::IsEnabled() {
  return base::FeatureList::IsEnabled(ash::features::kSystemExtensions);
}

SystemExtensionsProvider::SystemExtensionsProvider() {
  SystemExtensionsWebUIConfigMap::RegisterInstance();
  install_manager_ = std::make_unique<SystemExtensionsInstallManager>();
}

SystemExtensionsProvider::~SystemExtensionsProvider() = default;
