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
// should be called on the UI thread. If `record_full_metrics` is true then full
// metrics will be reported, otherwise only SupportLevel metric will be
// reported. `record_full_metrics` should only be set to true if app bound
// encryption APIs are not being used elsewhere, to avoid affecting the quality
// of the data.
bool MeasureAppBoundEncryptionStatus(PrefService* local_state,
                                     bool record_full_metrics);

}  // namespace os_crypt

#endif  // CHROME_BROWSER_OS_CRYPT_APP_BOUND_ENCRYPTION_METRICS_WIN_H_
