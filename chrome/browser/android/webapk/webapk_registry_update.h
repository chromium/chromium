// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_REGISTRY_UPDATE_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_REGISTRY_UPDATE_H_

#include <vector>

#include "chrome/browser/android/webapk/proto/webapk_database.pb.h"

namespace webapk {

// A raw registry update data.
struct RegistryUpdateData {
  RegistryUpdateData();
  RegistryUpdateData(const RegistryUpdateData&) = delete;
  RegistryUpdateData& operator=(const RegistryUpdateData&) = delete;
  ~RegistryUpdateData();

  using Apps = std::vector<std::unique_ptr<WebApkProto>>;
  Apps apps_to_create;
  Apps apps_to_update;

  std::vector<std::string> apps_to_delete;

  bool isEmpty() const;
};

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_REGISTRY_UPDATE_H_
