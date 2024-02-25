// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/install_prompt_permissions.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

InstallPromptPermissions::InstallPromptPermissions() = default;

InstallPromptPermissions::~InstallPromptPermissions() = default;

void InstallPromptPermissions::LoadFromPermissionSet(
    const PermissionSet* permissions_set,
    const Manifest::Type type) {
  const PermissionMessageProvider* message_provider =
      PermissionMessageProvider::Get();

  const PermissionMessages& permissions_messages =
      message_provider->GetPermissionMessages(
          message_provider->GetAllPermissionIDs(*permissions_set, type));

  AddPermissionMessages(permissions_messages);
}

void InstallPromptPermissions::AddPermissionMessages(
    const PermissionMessages& permissions_messages) {
  for (const PermissionMessage& msg : permissions_messages) {
    permissions.push_back(msg.message());
    // Add a dash to the front of each permission detail.
    std::u16string details_str;
    if (!msg.submessages().empty()) {
      std::vector<std::u16string> detail_lines_with_bullets;
      for (const auto& detail_line : msg.submessages()) {
        detail_lines_with_bullets.push_back(u"- " + detail_line);
      }

      details_str = base::JoinString(detail_lines_with_bullets, u"\n");
    }
    details.push_back(details_str);
    is_showing_details.push_back(false);
  }
}

}  // namespace extensions
