// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drivefs/drivefs_native_message_host_origins.h"

#include "base/containers/span.h"

namespace drive {

const char kDriveFsNativeMessageHostName[] = "com.google.drive.nativeproxy";

const base::span<const char* const> kDriveFsNativeMessageHostOrigins{{
    "chrome-extension://lmjegmlicamnimmfhcmpkclmigmmcbeh/",
}};

}  // namespace drive
