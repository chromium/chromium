// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/mock_extension_special_storage_policy.h"

MockExtensionSpecialStoragePolicy::MockExtensionSpecialStoragePolicy()
    : ExtensionSpecialStoragePolicy(NULL) {}

bool MockExtensionSpecialStoragePolicy::IsStorageProtected(const GURL& origin) {
  return protected_.find(origin) != protected_.end();
}

bool MockExtensionSpecialStoragePolicy::IsStorageUnlimited(const GURL& origin) {
  return false;
}

bool MockExtensionSpecialStoragePolicy::IsStorageSessionOnly(
    const GURL& origin) {
  return false;
}

bool MockExtensionSpecialStoragePolicy::HasSessionOnlyOrigins() {
  return false;
}

MockExtensionSpecialStoragePolicy::~MockExtensionSpecialStoragePolicy() {}
