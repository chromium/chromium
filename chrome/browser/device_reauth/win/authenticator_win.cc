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

#include "base/task/thread_pool.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "base/win/registry.h"
#include "base/win/scoped_hstring.h"
#include "base/win/scoped_winrt_initializer.h"
#include "base/win/windows_version.h"
#include "chrome/browser/password_manager/password_manager_util_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"

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

bool ResolveCoreWinRT() {
  return base::win::ResolveCoreWinRTDelayload() &&
         base::win::ScopedHString::ResolveCoreWinRTStringDelayload();
}

void GetAvailabilityValue(AvailabilityCallback callback,
                          UserConsentVerifierAvailability availability) {
  switch (availability) {
    case UserConsentVerifierAvailability_Available:
      std::move(callback).Run(true);
      break;
    case UserConsentVerifierAvailability_DeviceBusy:
    case UserConsentVerifierAvailability_DeviceNotPresent:
    case UserConsentVerifierAvailability_DisabledByPolicy:
    case UserConsentVerifierAvailability_NotConfiguredForUser:
      std::move(callback).Run(false);
      break;
  }
}

void OnAvailabilityReceived(scoped_refptr<base::SequencedTaskRunner> thread,
                            AvailabilityCallback callback,
                            UserConsentVerifierAvailability availability) {
  thread->PostTask(
      FROM_HERE,
      base::BindOnce(&GetAvailabilityValue, std::move(callback), availability));
}

void SetAvailability(scoped_refptr<base::SequencedTaskRunner> thread,
                     AvailabilityCallback callback,
                     bool availability) {
  thread->PostTask(FROM_HERE,
                   base::BindOnce(std::move(callback), availability));
}

// Asks operating system if user has configured and enabled Windows Hello on
// their machine. Runs `callback` on `thread`.
void GetBiometricAvailabilityFromWindows(
    AvailabilityCallback callback,
    scoped_refptr<base::SequencedTaskRunner> thread) {
  // UserConsentVerifier class is only available in Win 10 onwards.
  if (base::win::GetVersion() < base::win::Version::WIN10) {
    SetAvailability(thread, std::move(callback), false);
    return;
  }
  if (!ResolveCoreWinRT()) {
    SetAvailability(thread, std::move(callback), false);
    return;
  }
  ComPtr<IUserConsentVerifierStatics> factory;
  HRESULT hr = base::win::GetActivationFactory<
      IUserConsentVerifierStatics,
      RuntimeClass_Windows_Security_Credentials_UI_UserConsentVerifier>(
      &factory);
  if (FAILED(hr)) {
    SetAvailability(thread, std::move(callback), false);
    return;
  }
  ComPtr<IAsyncOperation<UserConsentVerifierAvailability>> async_op;
  hr = factory->CheckAvailabilityAsync(&async_op);
  if (FAILED(hr)) {
    SetAvailability(thread, std::move(callback), false);
    return;
  }

  base::win::PostAsyncResults(
      std::move(async_op),
      base::BindOnce(&OnAvailabilityReceived, thread, std::move(callback)));
}

}  // namespace

AuthenticatorWin::AuthenticatorWin() = default;

AuthenticatorWin::~AuthenticatorWin() = default;

bool AuthenticatorWin::AuthenticateUser(const std::u16string& message) {
  Browser* browser = chrome::FindBrowserWithActiveWindow();
  if (!browser)
    return false;

  gfx::NativeWindow window = browser->window()->GetNativeWindow();
  return password_manager_util_win::AuthenticateUser(window, message);
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
                     base::SequencedTaskRunnerHandle::Get()));
}
