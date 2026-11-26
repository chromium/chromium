// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNALS_APP_BOUND_ENCRYPTION_STATE_WIN_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNALS_APP_BOUND_ENCRYPTION_STATE_WIN_H_

#include "base/functional/callback_forward.h"
#include "components/device_signals/core/browser/signals_types.h"

class PrefService;

namespace os_crypt_async {
class OSCryptAsync;
}  // namespace os_crypt_async

namespace device_signals {

// Asynchronously determines the current `AppBoundEncryptionState` on Windows by
// checking `os_crypt_async` to verify whether the `"v20"` App-Bound Encryption
// key provider is actively used for encryption, and falling back to
// `os_crypt::GetAppBoundEncryptionSupportLevel()` with `local_state` when it is
// not.
//
// Note: `AppBoundEncryptionProviderWin` caches the support level at startup,
// and `ApplicationBoundEncryptionEnabled` has `dynamic_refresh: false`. If the
// policy is enabled at runtime after starting disabled, `"v20"` remains
// inactive while live `local_state` reports `kSupported`, so this function
// reports `kServiceFailure` until the browser restarts.
//
// Both `os_crypt_async` and `local_state` must be non-null, and `local_state`
// must outlive any pending `os_crypt_async` callback. Must be called on the UI
// thread.
void GetAppBoundEncryptionState(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    PrefService* local_state,
    base::OnceCallback<void(AppBoundEncryptionState)> callback);

}  // namespace device_signals

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNALS_APP_BOUND_ENCRYPTION_STATE_WIN_H_
