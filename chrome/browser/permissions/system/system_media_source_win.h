// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_MEDIA_SOURCE_WIN_H_
#define CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_MEDIA_SOURCE_WIN_H_

#include <windows.security.authorization.appcapabilityaccess.h>
#include <windows.system.h>
#include <wrl/client.h>

#include <optional>

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

  void SetMockStatus(ContentSettingsType type, std::optional<Status> status);

 private:
  SystemMediaSourceWin();
  SystemMediaSourceWin(const SystemMediaSourceWin&) = delete;
  SystemMediaSourceWin& operator=(const SystemMediaSourceWin&) = delete;
  ~SystemMediaSourceWin();
  friend class base::NoDestructor<SystemMediaSourceWin>;
  friend class SystemMediaSourceWinTest;

  void OnLaunchUriSuccess(uint8_t launched);
  void OnLaunchUriFailure(HRESULT result);

  // The pending operation for launching the settings page, or nullptr if not
  // launching the settings page.
  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncOperation<bool>>
      launch_uri_op_;

  std::optional<Status> camera_status_for_testing_;
  std::optional<Status> mic_status_for_testing_;

  base::WeakPtrFactory<SystemMediaSourceWin> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_MEDIA_SOURCE_WIN_H_
