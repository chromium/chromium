// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/extension_token_key.h"

#include <tuple>

namespace extensions {

ExtensionTokenKey::ExtensionTokenKey(const std::string& extension_id,
                                     const CoreAccountId& account_id,
                                     const std::set<std::string>& scopes)
    : extension_id(extension_id), account_id(account_id), scopes(scopes) {}

ExtensionTokenKey::ExtensionTokenKey(const ExtensionTokenKey& other) = default;

ExtensionTokenKey::~ExtensionTokenKey() {}

bool ExtensionTokenKey::operator<(const ExtensionTokenKey& rhs) const {
  return std::tie(extension_id, account_id, scopes) <
    std::tie(rhs.extension_id, rhs.account_id, rhs.scopes);
}

}  // namespace extensions
