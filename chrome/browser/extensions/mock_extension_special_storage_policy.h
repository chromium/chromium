// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MOCK_EXTENSION_SPECIAL_STORAGE_POLICY_H_
#define CHROME_BROWSER_EXTENSIONS_MOCK_EXTENSION_SPECIAL_STORAGE_POLICY_H_

#include <set>

#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "url/gurl.h"

// This class is the same as MockSpecialStoragePolicy (in
// content/public/test/mock_special_storage_policy.h), but it inherits
// ExtensionSpecialStoragePolicy instead of storage::SpecialStoragePolicy.
class MockExtensionSpecialStoragePolicy : public ExtensionSpecialStoragePolicy {
 public:
  MockExtensionSpecialStoragePolicy();

  MockExtensionSpecialStoragePolicy(const MockExtensionSpecialStoragePolicy&) =
      delete;
  MockExtensionSpecialStoragePolicy& operator=(
      const MockExtensionSpecialStoragePolicy&) = delete;

  // storage::SpecialStoragePolicy:
  bool IsStorageProtected(const GURL& origin) override;
  bool IsStorageUnlimited(const GURL& origin) override;
  bool IsStorageSessionOnly(const GURL& origin) override;
  bool HasSessionOnlyOrigins() override;

  void AddProtected(const GURL& origin) {
    protected_.insert(origin);
  }

 private:
  ~MockExtensionSpecialStoragePolicy() override;

  std::set<GURL> protected_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_MOCK_EXTENSION_SPECIAL_STORAGE_POLICY_H_
