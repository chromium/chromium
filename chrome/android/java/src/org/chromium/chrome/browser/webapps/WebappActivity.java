// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.components.webapk.lib.common.WebApkConstants.WEBAPK_PACKAGE_PREFIX;
import static org.chromium.webapk.lib.common.WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME;

import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappIntentUtils;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;

/** Displays a webapp in a nearly UI-less Chrome (InfoBars still appear). */
public class WebappActivity extends BaseCustomTabActivity {
    public static final String WEBAPP_SCHEME = "webapp";

    private static BrowserServicesIntentDataProvider sIntentDataProviderForTesting;

    @Override
    protected BrowserServicesIntentDataProvider buildIntentDataProvider(
            Intent intent, @CustomTabsIntent.ColorScheme int colorScheme) {
        if (intent == null) return null;

        if (sIntentDataProviderForTesting != null) {
            return sIntentDataProviderForTesting;
        }

        return TextUtils.isEmpty(WebappIntentUtils.getWebApkPackageName(intent))
                ? WebappIntentDataProviderFactory.create(intent)
                : WebApkIntentDataProviderFactory.create(intent);
    }

    public static void setIntentDataProviderForTesting(
            BrowserServicesIntentDataProvider intentDataProvider) {
        sIntentDataProviderForTesting = intentDataProvider;
        ResettersForTesting.register(() -> sIntentDataProviderForTesting = null);
    }

    @Override
    public boolean shouldPreferLightweightFre(Intent intent) {
        // We cannot get WebAPK package name from BrowserServicesIntentDataProvider because
        // {@link WebappActivity#performPreInflationStartup()} may not have been called yet.
        String webApkPackageName =
                IntentUtils.safeGetStringExtra(intent, EXTRA_WEBAPK_PACKAGE_NAME);

        // Use the lightweight FRE for unbound WebAPKs.
        return webApkPackageName != null && !webApkPackageName.startsWith(WEBAPK_PACKAGE_PREFIX);
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();
        getFullscreenManager().exitPersistentFullscreenMode();
    }

    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        // Disable creating bookmark.
        if (id == R.id.bookmark_this_page_id) {
            return true;
        }
        if (id == R.id.open_in_browser_id) {
            mNavigationController.openCurrentUrlInBrowser();
            if (fromMenu) {
                RecordUserAction.record("WebappMenuOpenInChrome");
            } else {
                RecordUserAction.record("Webapp.NotificationOpenInChrome");
            }
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu);
    }

    @Override
    protected Drawable getBackgroundDrawable() {
        return null;
    }

    @Override
    protected LaunchCauseMetrics createLaunchCauseMetrics() {
        return new WebappLaunchCauseMetrics(
                this,
                mWebappActivityCoordinator == null
                        ? null
                        : mWebappActivityCoordinator.getWebappInfo());
    }
}
