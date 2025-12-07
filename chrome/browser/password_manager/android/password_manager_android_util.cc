// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_android_util.h"

#include <string>

#include "base/android/device_info.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/password_manager/android/password_manager_util_bridge_interface.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"

namespace password_manager_android_util {

namespace {

bool HasMinGmsVersionForFullUpmSupport() {
  std::string gms_version_str = base::android::device_info::gms_version_code();
  int gms_version = 0;
  // gms_version_code() must be converted to int for comparison, because it can
  // have legacy values "3(...)" and those evaluate > "2023(...)".
  return base::StringToInt(gms_version_str, &gms_version) &&
         gms_version >= password_manager::GetSplitStoresUpmMinVersion();
}

}  // namespace

bool IsPasswordManagerAvailable(
    std::unique_ptr<PasswordManagerUtilBridgeInterface> util_bridge) {
  return IsPasswordManagerAvailable(util_bridge->IsInternalBackendPresent());
}

bool IsPasswordManagerAvailable(bool is_internal_backend_present) {
  if (!is_internal_backend_present) {
    return false;
  }

  if (!HasMinGmsVersionForFullUpmSupport()) {
    return false;
  }

  return true;
}

}  // namespace password_manager_android_util
