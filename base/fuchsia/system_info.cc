// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/system_info.h"

#include <fuchsia/buildinfo/cpp/fidl.h>
#include <fuchsia/hwinfo/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/threading/scoped_blocking_call.h"

namespace base {

namespace {

// Returns this process's cached object for `BuildInfo`.
fuchsia::buildinfo::BuildInfo& CachedBuildInfo() {
  static NoDestructor<fuchsia::buildinfo::BuildInfo> build_info;
  return *build_info;
}

// Synchronously fetches BuildInfo from the system and caches it for use in this
// process.
// Returns whether the system info was successfully cached.
bool FetchAndCacheBuildInfo() {
  DCHECK(CachedBuildInfo().IsEmpty()) << "Only call once per process";

  fuchsia::buildinfo::ProviderSyncPtr provider_sync;
  ComponentContextForProcess()->svc()->Connect(provider_sync.NewRequest());

  zx_status_t status = provider_sync->GetBuildInfo(&CachedBuildInfo());
  ZX_DLOG_IF(ERROR, status != ZX_OK, status);
  DLOG_IF(ERROR, CachedBuildInfo().IsEmpty()) << "Received empty BuildInfo";
  return status == ZX_OK && !CachedBuildInfo().IsEmpty();
}

}  // namespace

bool FetchAndCacheSystemInfo() {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::WILL_BLOCK);
  return FetchAndCacheBuildInfo();
}

const fuchsia::buildinfo::BuildInfo& GetCachedBuildInfo() {
  DCHECK(!CachedBuildInfo().IsEmpty())
      << "FetchAndCacheSystemInfo() has not been called in this process";
  return CachedBuildInfo();
}

// Synchronously fetches ProductInfo from the system
fuchsia::hwinfo::ProductInfo GetProductInfo() {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::WILL_BLOCK);
  fuchsia::hwinfo::ProductSyncPtr provider_sync;
  ComponentContextForProcess()->svc()->Connect(provider_sync.NewRequest());

  fuchsia::hwinfo::ProductInfo product_info;
  [[maybe_unused]] zx_status_t status = provider_sync->GetInfo(&product_info);
  ZX_DLOG_IF(ERROR, status != ZX_OK, status);
  return product_info;
}

void ClearCachedSystemInfoForTesting() {
  CachedBuildInfo() = {};
}

}  // namespace base
