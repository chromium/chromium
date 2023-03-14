// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/system_info.h"

#include <fidl/fuchsia.buildinfo/cpp/fidl.h>
#include <fidl/fuchsia.hwinfo/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include "base/check.h"
#include "base/fuchsia/fuchsia_component_connect.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/threading/scoped_blocking_call.h"

namespace base {

namespace {

// Returns this process's cached object for `BuildInfo`.
fuchsia_buildinfo::BuildInfo& CachedBuildInfo() {
  static NoDestructor<fuchsia_buildinfo::BuildInfo> build_info;
  return *build_info;
}

// Synchronously fetches BuildInfo from the system and caches it for use in this
// process.
// Returns whether the system info was successfully cached.
bool FetchAndCacheBuildInfo() {
  DCHECK(CachedBuildInfo().IsEmpty()) << "Only call once per process";

  auto provider_client_end =
      fuchsia_component::Connect<fuchsia_buildinfo::Provider>();
  if (provider_client_end.is_error()) {
    DLOG(ERROR) << base::FidlConnectionErrorMessage(provider_client_end);
    return false;
  }
  fidl::SyncClient provider_sync(std::move(provider_client_end.value()));

  auto build_info_result = provider_sync->GetBuildInfo();
  if (build_info_result.is_error()) {
    ZX_DLOG(ERROR, build_info_result.error_value().status());
    return false;
  }

  if (build_info_result->build_info().IsEmpty()) {
    DLOG(ERROR) << "Received empty BuildInfo";
    return false;
  }

  CachedBuildInfo() = std::move(build_info_result->build_info());
  return true;
}

}  // namespace

bool FetchAndCacheSystemInfo() {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::WILL_BLOCK);
  return FetchAndCacheBuildInfo();
}

const fuchsia_buildinfo::BuildInfo& GetCachedBuildInfo() {
  DCHECK(!CachedBuildInfo().IsEmpty())
      << "FetchAndCacheSystemInfo() has not been called in this process";
  return CachedBuildInfo();
}

// Synchronously fetches ProductInfo from the system
fuchsia_hwinfo::ProductInfo GetProductInfo() {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::WILL_BLOCK);
  auto product_client_end =
      fuchsia_component::Connect<fuchsia_hwinfo::Product>();
  if (product_client_end.is_error()) {
    DLOG(ERROR) << base::FidlConnectionErrorMessage(product_client_end);
    return {};
  }
  fidl::SyncClient provider_sync(std::move(product_client_end.value()));

  auto product_info_result = provider_sync->GetInfo();
  if (product_info_result.is_error()) {
    ZX_DLOG(ERROR, product_info_result.error_value().status());
    return {};
  }

  return product_info_result->info();
}

void ClearCachedSystemInfoForTesting() {
  CachedBuildInfo() = {};
}

}  // namespace base
