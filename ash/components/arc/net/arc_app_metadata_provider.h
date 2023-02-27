// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_NET_ARC_APP_METADATA_PROVIDER_H_
#define ASH_COMPONENTS_ARC_NET_ARC_APP_METADATA_PROVIDER_H_

#include <string>

namespace arc {

// Delegate class to fetch app metadata from Chrome browser's prefs.
class ArcAppMetadataProvider {
 public:
  virtual ~ArcAppMetadataProvider() = default;

  // Get ARC application package name from |app_id| through prefs.
  virtual std::string GetAppPackageName(const std::string& app_id) = 0;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_NET_ARC_APP_METADATA_PROVIDER_H_
