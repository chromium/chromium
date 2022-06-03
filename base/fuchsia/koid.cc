// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/koid.h"

#include "base/fuchsia/fuchsia_logging.h"

namespace base {

namespace {

absl::optional<zx_info_handle_basic_t> GetBasicInfo(
    const zx::object_base& handle) {
  zx_info_handle_basic_t basic;
  zx_status_t status = handle.get_info(ZX_INFO_HANDLE_BASIC, &basic,
                                       sizeof(basic), nullptr, nullptr);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_object_get_info";
    return {};
  }

  return basic;
}

}  // namespace

absl::optional<zx_koid_t> GetKoid(const zx::object_base& handle) {
  auto basic_info = GetBasicInfo(handle);
  if (!basic_info)
    return {};
  return basic_info->koid;
}

absl::optional<zx_koid_t> GetRelatedKoid(const zx::object_base& handle) {
  auto basic_info = GetBasicInfo(handle);
  if (!basic_info)
    return {};
  return basic_info->related_koid;
}

}  // namespace base
