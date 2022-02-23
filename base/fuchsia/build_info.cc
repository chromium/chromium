// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/build_info.h"

#include <fuchsia/buildinfo/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/no_destructor.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"

namespace base {

namespace {

fuchsia::buildinfo::BuildInfo FetchSystemBuildInfo() {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::WILL_BLOCK);

  fuchsia::buildinfo::ProviderSyncPtr build_info_provider_sync;
  ComponentContextForProcess()->svc()->Connect(
      build_info_provider_sync.NewRequest());

  fuchsia::buildinfo::BuildInfo build_info;
  zx_status_t status = build_info_provider_sync->GetBuildInfo(&build_info);
  ZX_DCHECK(status == ZX_OK, status);
  DCHECK(!build_info.IsEmpty()) << "FIDL service returned empty BuildInfo";
  return build_info;
}

// Returns this process's BuildInfo object.
fuchsia::buildinfo::BuildInfo& CachedBuildInfo() {
  static NoDestructor<fuchsia::buildinfo::BuildInfo> build_info;
  return *build_info;
}

}  // namespace

void FetchAndCacheSystemBuildInfo() {
  DCHECK(CachedBuildInfo().IsEmpty()) << "Only call once per process";
  CachedBuildInfo() = FetchSystemBuildInfo();
}

const fuchsia::buildinfo::BuildInfo& GetCachedBuildInfo() {
  DCHECK(!CachedBuildInfo().IsEmpty())
      << "FetchAndCacheSystemBuildInfo() has not been called in this process";
  return CachedBuildInfo();
}

StringPiece GetBuildInfoVersion() {
  return GetCachedBuildInfo().version();
}

void ClearCachedBuildInfoForTesting() {
  CachedBuildInfo() = {};
}

}  // namespace base
