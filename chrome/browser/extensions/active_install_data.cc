// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/active_install_data.h"

#include "extensions/common/extension_id.h"

namespace extensions {

ActiveInstallData::ActiveInstallData(const ExtensionId& extension_id)
    : extension_id(extension_id) {}

}  // namespace extensions
