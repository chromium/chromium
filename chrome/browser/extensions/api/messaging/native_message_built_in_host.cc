// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/api/messaging/native_message_built_in_host.h"

#include <string>

#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/url_pattern.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace extensions {

namespace {

bool MatchesSecurityOrigin(const NativeMessageBuiltInHost& host,
                           const ExtensionId& extension_id) {
  GURL origin(std::string(kExtensionScheme) + "://" + extension_id);
  for (size_t i = 0; i < host.allowed_origins_count; i++) {
    URLPattern allowed_origin(URLPattern::SCHEME_ALL, host.allowed_origins[i]);
    if (allowed_origin.MatchesSecurityOrigin(origin)) {
      return true;
    }
  }
  return false;
}

}  // namespace

std::unique_ptr<NativeMessageHost> NativeMessageHost::Create(
    content::BrowserContext* browser_context,
    gfx::NativeView native_view,
    const ExtensionId& source_extension_id,
    const std::string& native_host_name,
    bool allow_user_level,
    std::string* error) {
  for (size_t i = 0; i < kBuiltInHostsCount; i++) {
    const auto& host = kBuiltInHosts[i];
    if (host.name == native_host_name) {
      if (MatchesSecurityOrigin(host, source_extension_id)) {
        return (*host.create_function)(browser_context);
      }
      *error = kForbiddenError;
      return nullptr;
    }
  }
  *error = kNotFoundError;
  return nullptr;
}

}  // namespace extensions
