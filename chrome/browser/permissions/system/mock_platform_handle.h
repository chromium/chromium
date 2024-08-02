// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_SYSTEM_MOCK_PLATFORM_HANDLE_H_
#define CHROME_BROWSER_PERMISSIONS_SYSTEM_MOCK_PLATFORM_HANDLE_H_

#include "chrome/browser/permissions/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace system_permission_settings {

class MockPlatformHandle : public PlatformHandle {
 public:
  MockPlatformHandle();
  MockPlatformHandle(MockPlatformHandle&) = delete;
  MockPlatformHandle& operator=(MockPlatformHandle&) = delete;
  ~MockPlatformHandle() override;

  MOCK_METHOD(bool, CanPrompt, (ContentSettingsType type), (override));
  MOCK_METHOD(bool, IsDenied, (ContentSettingsType type), (override));
  MOCK_METHOD(bool, IsAllowed, (ContentSettingsType type), (override));
  MOCK_METHOD(void,
              OpenSystemSettings,
              (content::WebContents * web_contents, ContentSettingsType type),
              (override));
  MOCK_METHOD(void,
              Request,
              (ContentSettingsType type,
               SystemPermissionResponseCallback callback),
              (override));
  MOCK_METHOD(std::unique_ptr<ScopedObservation>,
              Observe,
              (SystemPermissionChangedCallback observer),
              (override));
};

}  // namespace system_permission_settings

#endif  // CHROME_BROWSER_PERMISSIONS_SYSTEM_MOCK_PLATFORM_HANDLE_H_
