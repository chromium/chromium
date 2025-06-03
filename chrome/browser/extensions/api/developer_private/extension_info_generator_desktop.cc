// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/extension_info_generator_desktop.h"

#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/extension_action/action_info.h"

namespace extensions {

namespace developer = api::developer_private;

ExtensionInfoGenerator::ExtensionInfoGenerator(
    content::BrowserContext* browser_context)
    : ExtensionInfoGeneratorShared(browser_context) {}

ExtensionInfoGenerator::~ExtensionInfoGenerator() = default;

void ExtensionInfoGenerator::FillExtensionInfo(
    const Extension& extension,
    api::developer_private::ExtensionState state,
    api::developer_private::ExtensionInfo info) {
  // Call the super class implementation to fill the rest of the struct.
  ExtensionInfoGeneratorShared::FillExtensionInfo(extension, state,
                                                  std::move(info));
}

}  // namespace extensions
