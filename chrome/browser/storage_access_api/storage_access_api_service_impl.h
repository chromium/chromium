// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_SERVICE_IMPL_H_
#define CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_SERVICE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "chrome/browser/storage_access_api/site_pair_cache.h"
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
  std::optional<base::TimeDelta> RenewPermissionGrant(
      const url::Origin& embedded_origin,
      const url::Origin& top_frame_origin) override;

  // KeyedService:
  void Shutdown() override;

  // Returns whether or not the repeating timer is running, for the purpose of
  // testing.
  bool IsTimerRunningForTesting() const;

 private:
  // Handles state updates due to the passage of time.
  void OnPeriodicTimerFired();

  // The BrowserContext this service instance is associated with.
  const raw_ref<content::BrowserContext> browser_context_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The grants that have already been updated.
  SitePairCache updated_grants_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer to periodically update state for the associated profile.
  base::RepeatingTimer periodic_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<StorageAccessAPIServiceImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_SERVICE_IMPL_H_
