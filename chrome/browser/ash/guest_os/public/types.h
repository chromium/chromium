// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_TYPES_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_TYPES_H_

#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

#include <string>

namespace vm_tools::concierge {
enum VmInfo_VmType : int;
}

namespace guest_os {

// When launching apps, either a file path or a string arg like "-h".
using LaunchArg = absl::variant<storage::FileSystemURL, std::string>;

using VmType = vm_tools::apps::VmType;

// Converts the concierge-specific type to the one we use in chrome (apps::).
VmType ToVmType(vm_tools::concierge::VmInfo_VmType type);

// Converts VmType to AppType.
apps::AppType ToAppType(vm_tools::apps::VmType type);

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_TYPES_H_
