// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_SERVICE_IMPL_H_
#define CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_SERVICE_IMPL_H_

#include "base/sequence_checker.h"
#include "chrome/browser/storage_access_api/storage_access_api_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

// A profile keyed service for Storage Access API state.
//
// This service always exists for a Profile, regardless of whether the Storage
// Access API feature is enabled.
class StorageAccessAPIServiceImpl : public StorageAccessAPIService,
                                    public KeyedService {
 public:
  explicit StorageAccessAPIServiceImpl(content::BrowserContext* context);
  StorageAccessAPIServiceImpl(const StorageAccessAPIServiceImpl&) = delete;
  StorageAccessAPIServiceImpl& operator=(const StorageAccessAPIServiceImpl&) =
      delete;
  ~StorageAccessAPIServiceImpl() override;

  // StorageAccessAPIService:
  void RenewPermissionGrant(const url::Origin& embedded_origin,
                            const url::Origin& top_frame_origin) override;

  // KeyedService:
  void Shutdown() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_SERVICE_IMPL_H_
