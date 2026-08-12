// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.components.webapk.lib.common.WebApkConstants.WEBAPK_PACKAGE_PREFIX;
import static org.chromium.webapk.lib.common.WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME;

import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.browserservices.intents.WebappIntentUtils;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;

/** Displays a webapp in a nearly UI-less Chrome (InfoBars still appear). */
@NullMarked
public class WebappActivity extends BaseCustomTabActivity {
    public static final String WEBAPP_SCHEME = "webapp";

    private static @Nullable BrowserServicesIntentDataProvider sIntentDataProviderForTesting;

    @Override
    protected @Nullable BrowserServicesIntentDataProvider buildIntentDataProvider(
            Intent intent, @CustomTabsIntent.ColorScheme int colorScheme) {
        if (intent == null) return null;

        if (sIntentDataProviderForTesting != null) {
            return sIntentDataProviderForTesting;
        }

        String webApkPackageName = WebappIntentUtils.getWebApkPackageName(intent);
        if (TextUtils.isEmpty(webApkPackageName) && getIntentDataProvider() != null) {
            WebApkExtras webApkExtras = getIntentDataProvider().getWebApkExtras();
            if (webApkExtras != null && !TextUtils.isEmpty(webApkExtras.webApkPackageName)) {
                webApkPackageName = webApkExtras.webApkPackageName;
                intent.putExtra(EXTRA_WEBAPK_PACKAGE_NAME, webApkPackageName);
            }
        }

        if (TextUtils.isEmpty(IntentUtils.safeGetStringExtra(intent, WebappConstants.EXTRA_URL))
                && intent.getData() != null) {
            intent.putExtra(WebappConstants.EXTRA_URL, intent.getDataString());
        }

        return TextUtils.isEmpty(webApkPackageName)
                ? WebappIntentDataProviderFactory.create(intent)
                : WebApkIntentDataProviderFactory.create(intent);
    }

    public static void setIntentDataProviderForTesting(
            BrowserServicesIntentDataProvider intentDataProvider) {
        sIntentDataProviderForTesting = intentDataProvider;
        ResettersForTesting.register(() -> sIntentDataProviderForTesting = null);
    }

    // When sWebAppShortEdgesCutoutMode is enabled, intentionally skip the activity-level
    // edge-to-edge token at creation time and let DisplayCutoutController acquire it later,
    // only after the page declares viewport-fit=cover. Drawing edge-to-edge unconditionally on
    // create would push standalone PWAs under the status bar even when the page never opted in.
    @Override
    protected boolean shouldDrawEdgeToEdgeOnCreate() {
        return !ChromeFeatureList.sWebAppShortEdgesCutoutMode.isEnabled()
                && super.shouldDrawEdgeToEdgeOnCreate();
    }

    @Override
    protected boolean canColorStatusBarWithEdgeToEdgeHelper() {
        return ChromeFeatureList.sWebAppShortEdgesCutoutMode.isEnabled()
                || super.canColorStatusBarWithEdgeToEdgeHelper();
    }

    @Override
    protected boolean canSetTransparentStatusBarWithoutDelegate() {
        return ChromeFeatureList.sWebAppShortEdgesCutoutMode.isEnabled();
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
    public boolean onMenuOrKeyboardAction(
            int id,
            boolean fromMenu,
            @Nullable Bundle menuItemData,
            @Nullable MotionEventInfo triggeringMotion) {
        // Disable creating bookmark.
        if (id == R.id.bookmark_this_page_id) {
            return true;
        }
        if (id == R.id.open_in_browser_id) {
            getCustomTabActivityNavigationController().openCurrentUrlInBrowser();
            if (fromMenu) {
                RecordUserAction.record("WebappMenuOpenInChrome");
            } else {
                RecordUserAction.record("Webapp.NotificationOpenInChrome");
            }
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu, menuItemData, triggeringMotion);
    }

    @Override
    protected @Nullable Drawable getBackgroundDrawable() {
        return null;
    }

    @Override
    protected LaunchCauseMetrics createLaunchCauseMetrics() {
        return new WebappLaunchCauseMetrics(
                this,
                getWebappActivityCoordinator() == null
                        ? null
                        : getWebappActivityCoordinator().getWebappInfo());
    }
}
