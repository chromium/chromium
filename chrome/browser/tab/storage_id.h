// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_STORAGE_ID_H_
#define CHROME_BROWSER_TAB_STORAGE_ID_H_

namespace tabs {

// Define a type alias for storage IDs. The underlying type may change before
// launch. Using a type alias allows for easier migration.
using StorageId = int;

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_STORAGE_ID_H_
