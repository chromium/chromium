// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OS_CRYPT_APP_BOUND_ENCRYPTION_METRICS_WIN_H_
#define CHROME_BROWSER_OS_CRYPT_APP_BOUND_ENCRYPTION_METRICS_WIN_H_

class PrefRegistrySimple;
class PrefService;

namespace os_crypt {

// Register local state prefs required by the app bound encryption metrics.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Posts background tasks to measure the app bound encryption metrics. This
// should be called on the UI thread.
bool MeasureAppBoundEncryptionStatus(PrefService* local_state);

}  // namespace os_crypt

#endif  // CHROME_BROWSER_OS_CRYPT_APP_BOUND_ENCRYPTION_METRICS_WIN_H_
