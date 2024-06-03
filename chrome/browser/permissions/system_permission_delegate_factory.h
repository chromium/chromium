// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_SYSTEM_PERMISSION_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_PERMISSIONS_SYSTEM_PERMISSION_DELEGATE_FACTORY_H_

#include <memory>

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt.h"
#include "components/content_settings/core/common/content_settings_types.h"

class SystemPermissionDelegateFactory {
 public:
  SystemPermissionDelegateFactory() = delete;
  SystemPermissionDelegateFactory(const SystemPermissionDelegateFactory&) =
      delete;
  SystemPermissionDelegateFactory& operator=(
      const SystemPermissionDelegateFactory) = delete;

  static std::unique_ptr<EmbeddedPermissionPrompt::SystemPermissionDelegate>
  CreateSystemPermissionDelegate(ContentSettingsType type);
};

#endif  // CHROME_BROWSER_PERMISSIONS_SYSTEM_PERMISSION_DELEGATE_FACTORY_H_
