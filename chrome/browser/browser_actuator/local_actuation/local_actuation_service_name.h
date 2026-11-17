// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACTUATOR_LOCAL_ACTUATION_LOCAL_ACTUATION_SERVICE_NAME_H_
#define CHROME_BROWSER_BROWSER_ACTUATOR_LOCAL_ACTUATION_LOCAL_ACTUATION_SERVICE_NAME_H_

#include <string>
#include <string_view>

namespace base {
class FilePath;
}  // namespace base

namespace browser_actuator {

// Returns the launchd bootstrap service name for `user_data_dir` and
// `base_bundle_id`: `<base_bundle_id>.local-actuation.<sha256(user_data_dir)>`,
// where the hash is lowercase hex over the path with trailing separators
// stripped.
std::string GetLocalActuationMachServiceName(
    std::string_view base_bundle_id,
    const base::FilePath& user_data_dir);

// As above, with `base::apple::BaseBundleID()` as `base_bundle_id`.
std::string GetLocalActuationMachServiceName(
    const base::FilePath& user_data_dir);

}  // namespace browser_actuator

#endif  // CHROME_BROWSER_BROWSER_ACTUATOR_LOCAL_ACTUATION_LOCAL_ACTUATION_SERVICE_NAME_H_
