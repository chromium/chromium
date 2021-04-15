// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_WEB_UI_CONFIG_MAP_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_WEB_UI_CONFIG_MAP_H_

#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "ui/webui/untrusted_web_ui_controller_factory.h"

// This class holds all WebUIConfigs for System Extensions and serves as a
// WebUIControllerFactory for requests to System Extension URLs.
class SystemExtensionsWebUIConfigMap
    : public ui::UntrustedWebUIControllerFactory {
 public:
  static SystemExtensionsWebUIConfigMap& GetInstance();
  static void RegisterInstance();

  SystemExtensionsWebUIConfigMap();
  SystemExtensionsWebUIConfigMap(const SystemExtensionsWebUIConfigMap&) =
      delete;
  SystemExtensionsWebUIConfigMap& operator=(
      const SystemExtensionsWebUIConfigMap&) = delete;

  void AddForSystemExtension(const SystemExtension& system_extension);

 protected:
  const WebUIConfigMap& GetWebUIConfigMap() override;

 private:
  ~SystemExtensionsWebUIConfigMap() override;

  WebUIConfigMap configs_;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_WEB_UI_CONFIG_MAP_H_
