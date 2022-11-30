// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_observer.h"

namespace extensions {

InstallObserver::ExtensionInstallParams::ExtensionInstallParams(
    std::string extension_id,
    std::string extension_name,
    gfx::ImageSkia installing_icon,
    bool is_app,
    bool is_platform_app)
        : extension_id(extension_id),
          extension_name(extension_name),
          installing_icon(installing_icon),
          is_app(is_app),
          is_platform_app(is_platform_app) {}

}  // namespace extensions
