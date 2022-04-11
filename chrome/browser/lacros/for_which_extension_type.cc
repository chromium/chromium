// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/for_which_extension_type.h"

#include "base/logging.h"
#include "chrome/browser/lacros/lacros_extensions_util.h"
#include "extensions/common/extension.h"

/******** ForWhichExtensionType ********/

ForWhichExtensionType::ForWhichExtensionType(bool for_chrome_apps)
    : for_chrome_apps_(for_chrome_apps) {}

ForWhichExtensionType::ForWhichExtensionType(const ForWhichExtensionType& inst)
    : ForWhichExtensionType(inst.for_chrome_apps_) {}

ForWhichExtensionType::~ForWhichExtensionType() = default;

bool ForWhichExtensionType::Matches(
    const extensions::Extension* extension) const {
  return for_chrome_apps_ ? lacros_extensions_util::IsExtensionApp(extension)
                          : extension->is_extension();
}

/******** Utilities ********/

ForWhichExtensionType InitForChromeApps() {
  return ForWhichExtensionType(/* for_chrome_apps_*/ true);
}

ForWhichExtensionType InitForExtensions() {
  return ForWhichExtensionType(/* for_chrome_apps_*/ false);
}
