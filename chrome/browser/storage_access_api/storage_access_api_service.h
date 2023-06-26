// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_SERVICE_H_
#define CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_SERVICE_H_

namespace url {
class Origin;
}  // namespace url

// An abstract class providing the interface for the Storage Access API service.
// This class exists so that a different implementation may be used in tests.
class StorageAccessAPIService {
 public:
  // May renew Storage Access API permission grants associated with the given
  // origins. Should return true if any grant was renewed, for ease of testing.
  //
  // The implementations of this method may apply rate limiting and caching in
  // order to avoid unnecessary disk writes.
  //
  // `embedded_origin` and `top_frame_origin` must be non-opaque.
  virtual bool RenewPermissionGrant(const url::Origin& embedded_origin,
                                    const url::Origin& top_frame_origin) = 0;
};

#endif  // CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_SERVICE_H_
