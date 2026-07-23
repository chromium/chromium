// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_MEDIA_PERMISSION_CACHE_MAC_TEST_HELPER_H_
#define CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_MEDIA_PERMISSION_CACHE_MAC_TEST_HELPER_H_

#include <memory>

namespace system_permission_settings {

class SystemMediaPermissionCacheMacTestHelper {
 public:
  SystemMediaPermissionCacheMacTestHelper();
  ~SystemMediaPermissionCacheMacTestHelper();

  void SetCameraStatus(bool denied);
  void SetMicStatus(bool denied);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace system_permission_settings

#endif  // CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_MEDIA_PERMISSION_CACHE_MAC_TEST_HELPER_H_
