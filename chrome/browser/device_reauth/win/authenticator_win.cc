// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/win/authenticator_win.h"

#include <objbase.h>

#include <UserConsentVerifierInterop.h>
#include <windows.foundation.h>
#include <windows.security.credentials.ui.h>
#include <windows.storage.streams.h>
#include <wrl/client.h>
#include <wrl/event.h>

#include <string>
#include <utility>

#include "authenticator_win.h"
#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"
#include "base/win/post_async_results.h"
#include "base/win/registry.h"
#include "base/win/scoped_hstring.h"
#include "base/win/scoped_winrt_initializer.h"
#include "base/win/windows_types.h"
#include "base/win/windows_version.h"
#include "chrome/browser/password_manager/password_manager_util_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "ui/aura/window.h"
#include "ui/views/win/hwnd_util.h"

namespace {

// Enum specifying possible states of Windows Hello authentication. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class AuthenticationStateWin {
  kStarted = 0,
  kFinished = 1,
  kMaxValue = kFinished,
};

using AvailabilityCallback = AuthenticatorWinInterface::AvailabilityCallback;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Security::Credentials::UI::IUserConsentVerifierStatics;
using ABI::Windows::Security::Credentials::UI::UserConsentVerificationResult;
using ABI::Windows::Security::Credentials::UI::UserConsentVerifierAvailability;
using enum ABI::Windows::Security::Credentials::UI::
    UserConsentVerifierAvailability;
using ABI::Windows::Security::Credentials::UI::UserConsentVerificationResult;
using enum ABI::Windows::Security::Credentials::UI::
    UserConsentVerificationResult;
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

AuthenticationResultStatusWin ConvertUserConsentVerificationResult(
    UserConsentVerificationResult result) {
  switch (result) {
    case UserConsentVerificationResult_Verified:
      return AuthenticationResultStatusWin::kVerified;
    case UserConsentVerificationResult_DeviceNotPresent:
      return AuthenticationResultStatusWin::kDeviceNotPresent;
    case UserConsentVerificationResult_NotConfiguredForUser:
      return AuthenticationResultStatusWin::kNotConfiguredForUser;
    case UserConsentVerificationResult_DisabledByPolicy:
      return AuthenticationResultStatusWin::kDisabledByPolicy;
    case UserConsentVerificationResult_DeviceBusy:
      return AuthenticationResultStatusWin::kDeviceBusy;
    case UserConsentVerificationResult_RetriesExhausted:
      return AuthenticationResultStatusWin::kRetriesExhausted;
    case UserConsentVerificationResult_Canceled:
      return AuthenticationResultStatusWin::kCanceled;
    default:
      return AuthenticationResultStatusWin::kUnknown;
  }
}

void RecordWindowsHelloAuthenticationResult(
    AuthenticationResultStatusWin result) {
  base::UmaHistogramEnumeration(
      "PasswordManager.RequestVerificationAsyncResult", result);
}

void RecordAuthenticationState(AuthenticationStateWin state) {
  base::UmaHistogramEnumeration("PasswordManager.AuthenticationStateWin",
                                state);
}

void RecordAuthenticationAsyncOpFailureReson(HRESULT hr) {
  base::UmaHistogramSparse("PasswordManager.AuthenticationAsyncOpFailureReson",
                           hr);
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

bool VectorToFirstElement(std::vector<bool> result) {
  CHECK_EQ(result.size(), 1u);
  return result[0];
}

// Asks operating system if user has configured and enabled Windows Hello on
// their machine. Runs `callback` on `thread`.
void GetBiometricAvailabilityFromWindows(
    AvailabilityCallback callback,
    scoped_refptr<base::SequencedTaskRunner> thread) {
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

void AuthenticateWithLegacyApi(const std::u16string& message,
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

void OnAuthenticationReceived(base::OnceCallback<void(bool)> callback,
                              const std::u16string& message,
                              UserConsentVerificationResult result) {
  AuthenticationResultStatusWin authentication_result =
      ConvertUserConsentVerificationResult(result);
  RecordWindowsHelloAuthenticationResult(authentication_result);

  switch (authentication_result) {
    case AuthenticationResultStatusWin::kVerified:
      std::move(callback).Run(/*success=*/true);
      return;
    case AuthenticationResultStatusWin::kCanceled:
    case AuthenticationResultStatusWin::kRetriesExhausted:
      std::move(callback).Run(/*success=*/false);
      return;
    case AuthenticationResultStatusWin::kDeviceNotPresent:
    case AuthenticationResultStatusWin::kNotConfiguredForUser:
    case AuthenticationResultStatusWin::kDisabledByPolicy:
    case AuthenticationResultStatusWin::kDeviceBusy:
    case AuthenticationResultStatusWin::kUnknown:
      // Windows Hello is not available so there should be a fallback to the old
      // API.
      AuthenticateWithLegacyApi(message, std::move(callback));
      return;
    case AuthenticationResultStatusWin::kFailedToCallAPI:
    case AuthenticationResultStatusWin::kFailedToCreateFactory:
    case AuthenticationResultStatusWin::kFailedToPostTask:
    case AuthenticationResultStatusWin::kAsyncOperationFailed:
    case AuthenticationResultStatusWin::kFailedToFindBrowser:
    case AuthenticationResultStatusWin::kFailedToFindHWNDForNativeWindow:
      // This values are not returned by UserConsentVerifier API.
      NOTREACHED();
  }
}

void OnAuthenticationAsyncOpFail(
    base::OnceCallback<void(bool)> callback,
    const std::u16string& message,
    HRESULT hr) {
  RecordWindowsHelloAuthenticationResult(
      AuthenticationResultStatusWin::kAsyncOperationFailed);
  RecordAuthenticationAsyncOpFailureReson(hr);

  AuthenticateWithLegacyApi(message, std::move(callback));
}

// TODO(b/349728186): Cleanup after Win11 solution is launched.
void PerformWindowsHelloAuthenticationAsync(
    base::OnceCallback<void(bool)> callback,
    const std::u16string& message) {
  ComPtr<IUserConsentVerifierStatics> factory;
  HRESULT hr = base::win::GetActivationFactory<
      IUserConsentVerifierStatics,
      RuntimeClass_Windows_Security_Credentials_UI_UserConsentVerifier>(
      &factory);
  if (FAILED(hr)) {
    RecordWindowsHelloAuthenticationResult(
        AuthenticationResultStatusWin::kFailedToCreateFactory);
    AuthenticateWithLegacyApi(message, std::move(callback));
    return;
  }
  ComPtr<IAsyncOperation<UserConsentVerificationResult>> async_op;
  hr = factory->RequestVerificationAsync(
      base::win::HStringReference(base::UTF16ToWide(message).c_str()).Get(),
      &async_op);
  if (FAILED(hr)) {
    RecordWindowsHelloAuthenticationResult(
        AuthenticationResultStatusWin::kFailedToCallAPI);
    AuthenticateWithLegacyApi(message, std::move(callback));
    return;
  }

  // In order to pass the callback to both the PostAsyncHandlers call and
  // to the AuthenticateWithLegacyApi (if the former fails to post the task) a
  // RepeatingCallback is needed. The callback will be executed at most once.
  auto barrier_callback = base::BarrierCallback<bool>(
      1, base::BindOnce(&VectorToFirstElement).Then(std::move(callback)));

  hr = base::win::PostAsyncHandlers(
      async_op.Get(),
      base::BindOnce(&OnAuthenticationReceived, barrier_callback, message),
      base::BindOnce(&OnAuthenticationAsyncOpFail, barrier_callback, message));

  if (FAILED(hr)) {
    RecordWindowsHelloAuthenticationResult(
        AuthenticationResultStatusWin::kFailedToPostTask);
    AuthenticateWithLegacyApi(message, barrier_callback);
    return;
  }
}

void PerformInteropWindowsHelloAuthenticationAsync(
    base::OnceCallback<void(bool)> callback,
    const std::u16string& message) {
  ComPtr<IUserConsentVerifierInterop> factory;
  HRESULT hr = base::win::GetActivationFactory<
      IUserConsentVerifierInterop,
      RuntimeClass_Windows_Security_Credentials_UI_UserConsentVerifier>(
      &factory);
  if (FAILED(hr)) {
    RecordWindowsHelloAuthenticationResult(
        AuthenticationResultStatusWin::kFailedToCreateFactory);
    AuthenticateWithLegacyApi(message, std::move(callback));
    return;
  }
  ComPtr<IAsyncOperation<UserConsentVerificationResult>> async_op;

  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    RecordWindowsHelloAuthenticationResult(
        AuthenticationResultStatusWin::kFailedToFindBrowser);
    AuthenticateWithLegacyApi(message, std::move(callback));
    return;
  }

  HWND hwnd = views::HWNDForNativeWindow(browser->window()->GetNativeWindow());
  if (!hwnd) {
    RecordWindowsHelloAuthenticationResult(
        AuthenticationResultStatusWin::kFailedToFindHWNDForNativeWindow);
    AuthenticateWithLegacyApi(message, std::move(callback));
    return;
  }

  hr = factory->RequestVerificationForWindowAsync(
      hwnd,
      base::win::HStringReference(base::UTF16ToWide(message).c_str()).Get(),
      __uuidof(IAsyncOperation<UserConsentVerificationResult>), &async_op);

  if (FAILED(hr)) {
    RecordWindowsHelloAuthenticationResult(
        AuthenticationResultStatusWin::kFailedToCallAPI);
    AuthenticateWithLegacyApi(message, std::move(callback));
    return;
  }

  // In order to pass the callback to both the PostAsyncHandlers call and
  // to the AuthenticateWithLegacyApi (if the former fails to post the task) a
  // RepeatingCallback is needed. The callback will be executed at most once.
  auto barrier_callback = base::BarrierCallback<bool>(
      1, base::BindOnce(&VectorToFirstElement).Then(std::move(callback)));

  hr = base::win::PostAsyncHandlers(
      async_op.Get(),
      base::BindOnce(&OnAuthenticationReceived, barrier_callback, message),
      base::BindOnce(&OnAuthenticationAsyncOpFail, barrier_callback, message));

  if (FAILED(hr)) {
    RecordWindowsHelloAuthenticationResult(
        AuthenticationResultStatusWin::kFailedToPostTask);
    AuthenticateWithLegacyApi(message, barrier_callback);
    return;
  }
}

void PerformWin11Authentication(
    const std::u16string& message,
    base::OnceCallback<void(bool)> result_callback) {
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kAuthenticateUsingUserConsentVerifierInteropApi)) {
    PerformInteropWindowsHelloAuthenticationAsync(std::move(result_callback),
                                                  message);
    return;
  }
  AuthenticateWithLegacyApi(message, std::move(result_callback));
}

void PerformWin10Authentication(
    const std::u16string& message,
    base::OnceCallback<void(bool)> result_callback) {
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kAuthenticateUsingUserConsentVerifierApi)) {
    // Posting authentication using the new API on a background thread causes
    // Windows Hello dialog not to attach to Chrome's UI and instead it is
    // visible behind it. Running it on the default thread isn't that bad
    // because the thread itself is not blocked and there are operations
    // happening while the win hello dialog is visible.
    PerformWindowsHelloAuthenticationAsync(std::move(result_callback), message);
    return;
  }
  AuthenticateWithLegacyApi(message, std::move(result_callback));
}

}  // namespace

AuthenticatorWin::AuthenticatorWin() = default;

AuthenticatorWin::~AuthenticatorWin() = default;

void AuthenticatorWin::AuthenticateUser(
    const std::u16string& message,
    base::OnceCallback<void(bool)> result_callback) {
  RecordAuthenticationState(AuthenticationStateWin::kStarted);

  // TODO(b/349728186): Cleanup after Win11 solution is launched.
  if (base::win::GetVersion() >= base::win::Version::WIN11) {
    PerformWin11Authentication(
        message, std::move(result_callback)
                     .Then(base::BindOnce(RecordAuthenticationState,
                                          AuthenticationStateWin::kFinished)));
    return;
  }

  PerformWin10Authentication(
      message, std::move(result_callback)
                   .Then(base::BindOnce(RecordAuthenticationState,
                                        AuthenticationStateWin::kFinished)));
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

bool AuthenticatorWin::CanAuthenticateWithScreenLock() {
  return password_manager_util_win::CanAuthenticateWithScreenLock();
}
