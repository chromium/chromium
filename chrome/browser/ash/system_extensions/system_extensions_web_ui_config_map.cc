// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_web_ui_config_map.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "chrome/browser/ash/system_extensions/system_extensions_ui.h"
#include "ui/webui/webui_config.h"

// static
SystemExtensionsWebUIConfigMap& SystemExtensionsWebUIConfigMap::GetInstance() {
  static base::NoDestructor<SystemExtensionsWebUIConfigMap> instance;
  return *instance.get();
}

// static
void SystemExtensionsWebUIConfigMap::RegisterInstance() {
  content::WebUIControllerFactory::RegisterFactory(
      &SystemExtensionsWebUIConfigMap::GetInstance());
}

SystemExtensionsWebUIConfigMap::SystemExtensionsWebUIConfigMap() = default;

SystemExtensionsWebUIConfigMap::~SystemExtensionsWebUIConfigMap() = default;

void SystemExtensionsWebUIConfigMap::AddForSystemExtension(
    const SystemExtension& system_extension) {
  auto config = std::make_unique<SystemExtensionsWebUIConfig>(system_extension);
  configs_[system_extension.base_url.host()] = std::move(config);
}

const ui::UntrustedWebUIControllerFactory::WebUIConfigMap&
SystemExtensionsWebUIConfigMap::GetWebUIConfigMap() {
  return configs_;
}
