// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CDM_STORAGE_ID_KEY_H_
#define CHROME_BROWSER_MEDIA_CDM_STORAGE_ID_KEY_H_

#include <string>

// Minimum length for the CDM Storage ID Key.
static const size_t kMinimumCdmStorageIdKeyLength = 32;

// Returns a browser specific value of at least |kMinimumCdmStorageKeyLength|
// characters, which will be used in the computation of the CDM Storage ID.
std::string GetCdmStorageIdKey();

#endif  // CHROME_BROWSER_MEDIA_CDM_STORAGE_ID_KEY_H_
