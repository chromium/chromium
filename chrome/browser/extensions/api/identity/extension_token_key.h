// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_EXTENSION_TOKEN_KEY_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_EXTENSION_TOKEN_KEY_H_

#include <set>
#include <string>

#include "google_apis/gaia/core_account_id.h"

namespace extensions {

struct ExtensionTokenKey {
  ExtensionTokenKey(const std::string& extension_id,
                    const CoreAccountId& account_id,
                    const std::set<std::string>& scopes);
  ExtensionTokenKey(const ExtensionTokenKey& other);
  ~ExtensionTokenKey();
  bool operator<(const ExtensionTokenKey& rhs) const;
  std::string extension_id;
  CoreAccountId account_id;
  std::set<std::string> scopes;
};

}  // namespace extensions


#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_EXTENSION_TOKEN_KEY_H_
