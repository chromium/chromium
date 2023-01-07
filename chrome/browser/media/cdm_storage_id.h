// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CDM_STORAGE_ID_H_
#define CHROME_BROWSER_MEDIA_CDM_STORAGE_ID_H_

#include <stdint.h>

#include <vector>

#include "base/functional/callback_forward.h"

namespace url {
class Origin;
}

// This handles computing the Storage Id for platform verification.

using CdmStorageIdCallback =
    base::OnceCallback<void(const std::vector<uint8_t>& storage_id)>;

// Computes the Storage Id based on |profile_salt|, |origin|, and some
// platform specific values. This may be asynchronous, so call |callback|
// with the result. If Storage Id is not supported on the current platform,
// a compile time error will be generated.
void ComputeStorageId(const std::vector<uint8_t>& profile_salt,
                      const url::Origin& origin,
                      CdmStorageIdCallback callback);

#endif  // CHROME_BROWSER_MEDIA_CDM_STORAGE_ID_H_
