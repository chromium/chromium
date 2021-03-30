// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;

import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.browserservices.intents.WebApkDistributor;
import org.chromium.components.webapps.ShortcutSource;

/**
 * LaunchCauseMetrics for WebappActivity.
 */
public class WebappLaunchCauseMetrics extends LaunchCauseMetrics {
    private WebappInfo mWebappInfo;

    public WebappLaunchCauseMetrics(Activity activity, WebappInfo info) {
        super(activity);
        mWebappInfo = info;
    }

    @Override
    public @LaunchCause int computeIntentLaunchCause() {
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

        // EXTERNAL_INTENT_FROM_CHROME is used when Chrome chooses to launch a WebApk for a
        // navigation, which doesn't count as a launch since Chrome was already in the
        // foreground.
        assert mWebappInfo.source() != ShortcutSource.EXTERNAL_INTENT_FROM_CHROME;
        return LaunchCause.OTHER;
    }
}
