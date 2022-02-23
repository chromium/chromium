// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_profile_utils.h"

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"

constexpr char kSystemExtensionsProfileDirectory[] = "SystemExtensions";

base::FilePath GetDirectoryForSystemExtension(Profile& profile,
                                              const SystemExtensionId& id) {
  return GetSystemExtensionsProfileDir(profile).Append(
      SystemExtension::IdToString(id));
}

base::FilePath GetSystemExtensionsProfileDir(Profile& profile) {
  return profile.GetPath().Append(kSystemExtensionsProfileDirectory);
}
