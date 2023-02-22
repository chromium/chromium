// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/win/authenticator_win.h"

#include <objbase.h>
#include <windows.foundation.h>
#include <windows.security.credentials.ui.h>
#include <windows.storage.streams.h>
#include <wrl/client.h>
#include <wrl/event.h>

#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "base/win/registry.h"
#include "base/win/scoped_hstring.h"
#include "base/win/scoped_winrt_initializer.h"
#include "chrome/browser/password_manager/password_manager_util_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/aura/window.h"

namespace {

using AvailabilityCallback = AuthenticatorWinInterface::AvailabilityCallback;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Security::Credentials::UI::IUserConsentVerifierStatics;
using ABI::Windows::Security::Credentials::UI::UserConsentVerifierAvailability;
using ABI::Windows::Security::Credentials::UI::
    UserConsentVerifierAvailability_Available;
using ABI::Windows::Security::Credentials::UI::
    UserConsentVerifierAvailability_DeviceBusy;
using ABI::Windows::Security::Credentials::UI::
    UserConsentVerifierAvailability_DeviceNotPresent;
using ABI::Windows::Security::Credentials::UI::
    UserConsentVerifierAvailability_DisabledByPolicy;
using ABI::Windows::Security::Credentials::UI::
    UserConsentVerifierAvailability_NotConfiguredForUser;
using Microsoft::WRL::ComPtr;

BiometricAuthenticationStatusWin ConvertUserConsentVerifierAvailability(
    UserConsentVerifierAvailability availability) {
  switch (availability) {
    case UserConsentVerifierAvailability_Available:
      return BiometricAuthenticationStatusWin::kAvailable;
    case UserConsentVerifierAvailability_DeviceBusy:
      return BiometricAuthenticationStatusWin::kDeviceBusy;
    case UserConsentVerifierAvailability_DeviceNotPresent:
      return BiometricAuthenticationStatusWin::kDeviceNotPresent;
    case UserConsentVerifierAvailability_DisabledByPolicy:
      return BiometricAuthenticationStatusWin::kDisabledByPolicy;
    case UserConsentVerifierAvailability_NotConfiguredForUser:
      return BiometricAuthenticationStatusWin::kNotConfiguredForUser;
    default:
      return BiometricAuthenticationStatusWin::kUnknown;
  }
}

void ReturnAvailabilityValue(AvailabilityCallback callback,
                             UserConsentVerifierAvailability availability) {
  std::move(callback).Run(ConvertUserConsentVerifierAvailability(availability));
}

void OnAvailabilityReceived(scoped_refptr<base::SequencedTaskRunner> thread,
                            AvailabilityCallback callback,
                            UserConsentVerifierAvailability availability) {
  thread->PostTask(FROM_HERE,
                   base::BindOnce(&ReturnAvailabilityValue, std::move(callback),
                                  availability));
}

void ReportCantCheckAvailability(
    scoped_refptr<base::SequencedTaskRunner> thread,
    AvailabilityCallback callback) {
  thread->PostTask(FROM_HERE,
                   base::BindOnce(std::move(callback),
                                  BiometricAuthenticationStatusWin::kUnknown));
}

// Asks operating system if user has configured and enabled Windows Hello on
// their machine. Runs `callback` on `thread`.
void GetBiometricAvailabilityFromWindows(
    AvailabilityCallback callback,
    scoped_refptr<base::SequencedTaskRunner> thread) {
  if (!base::win::ResolveCoreWinRTDelayload()) {
    ReportCantCheckAvailability(thread, std::move(callback));
    return;
  }

  // Mitigate the issues caused by loading DLLs on a background thread
  // (http://crbug/973868).
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  ComPtr<IUserConsentVerifierStatics> factory;
  HRESULT hr = base::win::GetActivationFactory<
      IUserConsentVerifierStatics,
      RuntimeClass_Windows_Security_Credentials_UI_UserConsentVerifier>(
      &factory);
  if (FAILED(hr)) {
    ReportCantCheckAvailability(thread, std::move(callback));
    return;
  }
  ComPtr<IAsyncOperation<UserConsentVerifierAvailability>> async_op;
  hr = factory->CheckAvailabilityAsync(&async_op);
  if (FAILED(hr)) {
    ReportCantCheckAvailability(thread, std::move(callback));
    return;
  }

  base::win::PostAsyncResults(
      std::move(async_op),
      base::BindOnce(&OnAvailabilityReceived, thread, std::move(callback)));
}

}  // namespace

AuthenticatorWin::AuthenticatorWin() = default;

AuthenticatorWin::~AuthenticatorWin() = default;

void AuthenticatorWin::AuthenticateUser(
    const std::u16string& message,
    base::OnceCallback<void(bool)> result_callback) {
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(result_callback), /*success=*/false));
    return;
  }

  gfx::NativeWindow window = browser->window()->GetNativeWindow();

  base::SequencedTaskRunner::GetCurrentDefault()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&password_manager_util_win::AuthenticateUser, window,
                     message),
      std::move(result_callback));
}

void AuthenticatorWin::CheckIfBiometricsAvailable(
    AvailabilityCallback callback) {
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateCOMSTATaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  DCHECK(background_task_runner);
  background_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&GetBiometricAvailabilityFromWindows, std::move(callback),
                     base::SequencedTaskRunner::GetCurrentDefault()));
}
