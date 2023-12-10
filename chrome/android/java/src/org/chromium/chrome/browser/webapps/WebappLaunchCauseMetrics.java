// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.components.webapps.ShortcutSource;
import org.chromium.components.webapps.WebApkDistributor;

/** LaunchCauseMetrics for WebappActivity. */
public class WebappLaunchCauseMetrics extends LaunchCauseMetrics {
    @Nullable private WebappInfo mWebappInfo;

    public WebappLaunchCauseMetrics(Activity activity, @Nullable WebappInfo info) {
        super(activity);
        mWebappInfo = info;
    }

    @Override
    public @LaunchCause int computeIntentLaunchCause() {
        if (mWebappInfo == null) return LaunchCause.OTHER;
        if (mWebappInfo.isLaunchedFromHomescreen()) {
            if (mWebappInfo.isForWebApk()) {
                if (mWebappInfo.distributor() == WebApkDistributor.BROWSER) {
                    return LaunchCause.WEBAPK_CHROME_DISTRIBUTOR;
                }
                return LaunchCause.WEBAPK_OTHER_DISTRIBUTOR;
            }
            // Legacy PWAs added via Add to Homescreen, either because install failed or was
            // unsupported by the device/browser, or the icon pre-dates WebApks, or this is a
            // standalone fullscreen pwa without either a ServiceWorker or Manifest. Should be
            // mostly indistinguishable from an installed WebApk.
            return LaunchCause.WEBAPK_CHROME_DISTRIBUTOR;
        }

        if (mWebappInfo.source() == ShortcutSource.EXTERNAL_INTENT) {
            return LaunchCause.EXTERNAL_VIEW_INTENT;
        }

        return LaunchCause.OTHER;
    }
}
