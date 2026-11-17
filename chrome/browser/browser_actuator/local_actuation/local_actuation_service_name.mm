// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/local_actuation/local_actuation_service_name.h"

#include <string>
#include <string_view>

#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/hash.h"

namespace browser_actuator {

std::string GetLocalActuationMachServiceName(
    std::string_view base_bundle_id,
    const base::FilePath& user_data_dir) {
  const base::FilePath stripped = user_data_dir.StripTrailingSeparators();
  return base::StrCat(
      {base_bundle_id, ".local-actuation.",
       base::HexEncodeLower(crypto::hash::Sha256(stripped.value()))});
}

std::string GetLocalActuationMachServiceName(
    const base::FilePath& user_data_dir) {
  return GetLocalActuationMachServiceName(base::apple::BaseBundleID(),
                                          user_data_dir);
}

}  // namespace browser_actuator
