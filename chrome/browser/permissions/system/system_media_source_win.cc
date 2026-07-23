// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_media_source_win.h"

#include <objidl.h>
#include <windows.foundation.h>
#include <windows.media.capture.h>

#include <optional>
#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/strings/utf_string_conversions.h"
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

// COM message filter that suppresses window message dispatching during
// cross-apartment COM calls, preventing re-entrancy crashes.
class SuppressMessagePumpFilter
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMessageFilter> {
 public:
  IFACEMETHODIMP_(DWORD)
  HandleInComingCall(DWORD /*dwCallType*/,
                     HTASK /*htaskCaller*/,
                     DWORD /*dwTickCount*/,
                     LPINTERFACEINFO /*lpInterfaceInfo*/) override {
    return SERVERCALL_ISHANDLED;
  }

  IFACEMETHODIMP_(DWORD)
  RetryRejectedCall(HTASK /*htaskCallee*/,
                    DWORD /*dwTickCount*/,
                    DWORD /*dwRejectType*/) override {
    return 0;
  }

  IFACEMETHODIMP_(DWORD)
  MessagePending(HTASK /*htaskCallee*/,
                 DWORD /*dwTickCount*/,
                 DWORD /*dwPendingType*/) override {
    return PENDINGMSG_WAITNOPROCESS;
  }
};

SystemMediaSourceWin::Status SystemPermissionStatusImpl(
    std::string_view capability_name) {
  using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
      AppCapabilityAccessStatus;
  using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
      AppCapabilityAccessStatus_Allowed;
  using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
      AppCapabilityAccessStatus_UserPromptRequired;

  APTTYPE apt_type;
  APTTYPEQUALIFIER apt_qualifier;
  bool has_sta = SUCCEEDED(CoGetApartmentType(&apt_type, &apt_qualifier)) &&
                 apt_type == APTTYPE_STA;

  auto filter = Microsoft::WRL::Make<SuppressMessagePumpFilter>();
  ComPtr<IMessageFilter> old_filter;
  if (has_sta) {
    CoRegisterMessageFilter(filter.Get(), &old_filter);
  }

  ComPtr<IAppCapability> app_capability = CreateAppCapability(capability_name);

  // Make sure to check status before removing the filter.
  AppCapabilityAccessStatus access_status;
  HRESULT hr = S_OK;
  if (app_capability) {
    hr = app_capability->CheckAccess(&access_status);
  }

  if (has_sta) {
    CoRegisterMessageFilter(old_filter.Get(), nullptr);
  }

  if (!app_capability || FAILED(hr)) {
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

SystemMediaSourceWin::SystemMediaSourceWin() = default;

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
  if (type == ContentSettingsType::MEDIASTREAM_MIC &&
      mic_status_for_testing_.has_value()) {
    return mic_status_for_testing_.value();
  }
  if ((type == ContentSettingsType::MEDIASTREAM_CAMERA ||
       type == ContentSettingsType::CAMERA_PAN_TILT_ZOOM) &&
      camera_status_for_testing_.has_value()) {
    return camera_status_for_testing_.value();
  }

  switch (type) {
    case ContentSettingsType::MEDIASTREAM_MIC:
      return SystemPermissionStatusImpl("microphone");
    case ContentSettingsType::MEDIASTREAM_CAMERA:
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      return SystemPermissionStatusImpl("webcam");
    default:
      NOTREACHED();
  }
}

void SystemMediaSourceWin::SetMockStatus(ContentSettingsType type,
                                         std::optional<Status> status) {
  if (type == ContentSettingsType::MEDIASTREAM_MIC) {
    mic_status_for_testing_ = status;
  } else if (type == ContentSettingsType::MEDIASTREAM_CAMERA ||
             type == ContentSettingsType::CAMERA_PAN_TILT_ZOOM) {
    camera_status_for_testing_ = status;
  }
}
