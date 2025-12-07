// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_MEDIA_SOURCE_WIN_H_
#define CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_MEDIA_SOURCE_WIN_H_

#include <windows.security.authorization.appcapabilityaccess.h>
#include <windows.system.h>
#include <wrl/client.h>

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "components/content_settings/core/common/content_settings_types.h"

class SystemMediaSourceWin {
 public:
  static SystemMediaSourceWin& GetInstance();

  enum class Status {
    kNotDetermined = 0,
    kDenied = 1,
    kAllowed = 2,
  };

  void OpenSystemPermissionSetting(ContentSettingsType type);

  Status SystemPermissionStatus(ContentSettingsType type);

 private:
  SystemMediaSourceWin();
  SystemMediaSourceWin(const SystemMediaSourceWin&) = delete;
  SystemMediaSourceWin& operator=(const SystemMediaSourceWin&) = delete;
  ~SystemMediaSourceWin();
  friend class base::NoDestructor<SystemMediaSourceWin>;

  void OnLaunchUriSuccess(uint8_t launched);
  void OnLaunchUriFailure(HRESULT result);

  // The pending operation for launching the settings page, or nullptr if not
  // launching the settings page.
  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncOperation<bool>>
      launch_uri_op_;

  // AppCapability objects for camera and microphone
  Microsoft::WRL::ComPtr<ABI::Windows::Security::Authorization::
                             AppCapabilityAccess::IAppCapability>
      camera_capability_;
  Microsoft::WRL::ComPtr<ABI::Windows::Security::Authorization::
                             AppCapabilityAccess::IAppCapability>
      microphone_capability_;

  base::WeakPtrFactory<SystemMediaSourceWin> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_MEDIA_SOURCE_WIN_H_
