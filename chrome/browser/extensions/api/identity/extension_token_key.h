// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_EXTENSION_TOKEN_KEY_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_EXTENSION_TOKEN_KEY_H_

#include <set>
#include <string>

#include "components/signin/public/identity_manager/account_info.h"

namespace extensions {

struct ExtensionTokenKey {
  ExtensionTokenKey(const std::string& extension_id,
                    const CoreAccountInfo& account_info,
                    const std::set<std::string>& scopes);
  ExtensionTokenKey(const ExtensionTokenKey& other);
  ~ExtensionTokenKey();
  bool operator<(const ExtensionTokenKey& rhs) const;
  std::string extension_id;
  CoreAccountInfo account_info;
  std::set<std::string> scopes;
};

}  // namespace extensions


#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_EXTENSION_TOKEN_KEY_H_
