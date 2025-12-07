// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_media_source_win.h"

#include <windows.foundation.h>
#include <windows.media.capture.h>

#include <optional>
#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"

namespace {
using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
    IAppCapability;
using ::Microsoft::WRL::ComPtr;
// Create an AppCapability object for the capability named `name`.
ComPtr<IAppCapability> CreateAppCapability(std::string_view name) {
  using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
      IAppCapabilityStatics;
  ComPtr<IAppCapabilityStatics> app_capability_statics;
  HRESULT hr = base::win::GetActivationFactory<
      IAppCapabilityStatics,
      RuntimeClass_Windows_Security_Authorization_AppCapabilityAccess_AppCapability>(
      &app_capability_statics);
  if (FAILED(hr)) {
    return nullptr;
  }
  auto capability_name = base::win::ScopedHString::Create(name);
  ComPtr<IAppCapability> app_capability;
  hr = app_capability_statics->Create(capability_name.get(), &app_capability);
  if (FAILED(hr)) {
    return nullptr;
  }
  return app_capability;
}

SystemMediaSourceWin::Status SystemPermissionStatusImpl(
    ComPtr<IAppCapability> app_capability) {
  using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
      AppCapabilityAccessStatus;
  using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
      AppCapabilityAccessStatus_Allowed;
  using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
      AppCapabilityAccessStatus_UserPromptRequired;
  if (!app_capability) {
    return SystemMediaSourceWin::Status::kNotDetermined;
  }
  AppCapabilityAccessStatus access_status;
  HRESULT hr = app_capability->CheckAccess(&access_status);
  if (FAILED(hr)) {
    return SystemMediaSourceWin::Status::kNotDetermined;
  }

  if (access_status == AppCapabilityAccessStatus_Allowed) {
    return SystemMediaSourceWin::Status::kAllowed;
  }
  if (access_status == AppCapabilityAccessStatus_UserPromptRequired) {
    return SystemMediaSourceWin::Status::kNotDetermined;
  }
  return SystemMediaSourceWin::Status::kDenied;
}

}  // namespace

using ::Microsoft::WRL::ComPtr;

SystemMediaSourceWin::SystemMediaSourceWin()
    : camera_capability_(CreateAppCapability("webcam")),
      microphone_capability_(CreateAppCapability("Microphone")) {}

SystemMediaSourceWin::~SystemMediaSourceWin() = default;

// static
SystemMediaSourceWin& SystemMediaSourceWin::GetInstance() {
  static base::NoDestructor<SystemMediaSourceWin> instance;
  return *instance;
}

void SystemMediaSourceWin::OnLaunchUriSuccess(uint8_t launched) {
  launch_uri_op_.Reset();
}

void SystemMediaSourceWin::OnLaunchUriFailure(HRESULT result) {
  launch_uri_op_.Reset();
}

// static
void SystemMediaSourceWin::OpenSystemPermissionSetting(
    ContentSettingsType type) {
  using ABI::Windows::Foundation::IUriRuntimeClass;
  using ABI::Windows::Foundation::IUriRuntimeClassFactory;
  using ABI::Windows::System::ILauncherStatics;
  if (launch_uri_op_) {
    return;
  }

  std::string capability_name;

  switch (type) {
    case ContentSettingsType::MEDIASTREAM_MIC:
      capability_name = "ms-settings:privacy-microphone";
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      capability_name = "ms-settings:privacy-webcam";
      break;
    default:
      NOTREACHED();
  }

  ComPtr<IUriRuntimeClassFactory> uri_runtime_class_factory;
  HRESULT hr =
      base::win::GetActivationFactory<IUriRuntimeClassFactory,
                                      RuntimeClass_Windows_Foundation_Uri>(
          &uri_runtime_class_factory);
  if (FAILED(hr)) {
    return;
  }
  ComPtr<IUriRuntimeClass> uri_runtime_class;
  base::win::ScopedHString uri_string =
      base::win::ScopedHString::Create(capability_name);
  hr = uri_runtime_class_factory->CreateUri(uri_string.get(),
                                            &uri_runtime_class);
  if (FAILED(hr)) {
    return;
  }
  ComPtr<ILauncherStatics> launcher_statics;
  hr = base::win::GetActivationFactory<ILauncherStatics,
                                       RuntimeClass_Windows_System_Launcher>(
      &launcher_statics);
  if (FAILED(hr)) {
    return;
  }
  hr = launcher_statics->LaunchUriAsync(uri_runtime_class.Get(),
                                        &launch_uri_op_);
  if (FAILED(hr)) {
    return;
  }
  base::win::PostAsyncHandlers(
      launch_uri_op_.Get(),
      base::BindOnce(&SystemMediaSourceWin::OnLaunchUriSuccess,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&SystemMediaSourceWin::OnLaunchUriFailure,
                     weak_factory_.GetWeakPtr()));
}

SystemMediaSourceWin::Status SystemMediaSourceWin::SystemPermissionStatus(
    ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::MEDIASTREAM_MIC:
      return SystemPermissionStatusImpl(microphone_capability_);
    case ContentSettingsType::MEDIASTREAM_CAMERA:
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      return SystemPermissionStatusImpl(camera_capability_);
    default:
      NOTREACHED();
  }
}
