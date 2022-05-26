// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "base/no_destructor.h"
#include "base/threading/scoped_blocking_call.h"

namespace base {

namespace {

// Returns this process's ProductInfo object.
template <typename Data>
Data& CachedData() {
  static NoDestructor<Data> data;
  return *data;
}

template <typename Data>
const Data& GetCachedData() {
  DCHECK(!CachedData<Data>().IsEmpty())
      << "FetchAndCacheSystemInfo() has not been called in this process";
  return CachedData<Data>();
}

template <typename Interface,
          typename Data,
          zx_status_t (Interface::Sync_::*Getter)(Data*)>
void FetchAndCacheData() {
  DCHECK(CachedData<Data>().IsEmpty()) << "Only call once per process";

  fidl::SynchronousInterfacePtr<Interface> provider_sync;
  ComponentContextForProcess()->svc()->Connect(provider_sync.NewRequest());

  zx_status_t status = (provider_sync.get()->*Getter)(&CachedData<Data>());
  ZX_CHECK(status == ZX_OK, status) << Interface::Name_;
  DCHECK(!CachedData<Data>().IsEmpty()) << "FIDL service returned empty data";
}

}  // namespace

void FetchAndCacheSystemInfo() {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::WILL_BLOCK);
  FetchAndCacheData<fuchsia::buildinfo::Provider, fuchsia::buildinfo::BuildInfo,
                    &fuchsia::buildinfo::Provider_Sync::GetBuildInfo>();
  FetchAndCacheData<fuchsia::hwinfo::Product, fuchsia::hwinfo::ProductInfo,
                    &fuchsia::hwinfo::Product_Sync::GetInfo>();
}

const fuchsia::buildinfo::BuildInfo& GetCachedBuildInfo() {
  return GetCachedData<fuchsia::buildinfo::BuildInfo>();
}

const fuchsia::hwinfo::ProductInfo& GetCachedProductInfo() {
  return GetCachedData<fuchsia::hwinfo::ProductInfo>();
}

void ClearCachedSystemInfoForTesting() {
  CachedData<fuchsia::buildinfo::BuildInfo>() = {};
  CachedData<fuchsia::hwinfo::ProductInfo>() = {};
}

}  // namespace base
