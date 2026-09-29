// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.components.webapk.lib.common.WebApkConstants.WEBAPK_PACKAGE_PREFIX;
import static org.chromium.webapk.lib.common.WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME;

import android.content.Intent;
import android.graphics.drawable.ColorDrawable;
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
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.browserservices.intents.WebappIntentUtils;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.ui.util.ColorUtils;

/** Displays a webapp in a nearly UI-less Chrome (InfoBars still appear). */
@NullMarked
public class WebappActivity extends BaseCustomTabActivity {
    public static final String WEBAPP_SCHEME = "webapp";

    private static @Nullable BrowserServicesIntentDataProvider sIntentDataProviderForTesting;

    /**
     * Provider built before {@link #performPreInflationStartup()} so that the edge-to-edge
     * decisions below can read the display mode, along with the intent it was parsed from. It is
     * handed back to the {@link #buildIntentDataProvider} call for that same intent, so the intent
     * is only parsed once. {@code mBuiltStartupIntentDataProvider} is tracked separately because
     * parsing an invalid intent legitimately yields a null provider.
     */
    private @Nullable BrowserServicesIntentDataProvider mStartupIntentDataProvider;

    private @Nullable Intent mStartupIntent;
    private boolean mBuiltStartupIntentDataProvider;

    @Override
    protected @Nullable BrowserServicesIntentDataProvider buildIntentDataProvider(
            Intent intent, @CustomTabsIntent.ColorScheme int colorScheme) {
        if (mBuiltStartupIntentDataProvider && intent == mStartupIntent) {
            BrowserServicesIntentDataProvider provider = mStartupIntentDataProvider;
            mStartupIntentDataProvider = null;
            mStartupIntent = null;
            mBuiltStartupIntentDataProvider = false;
            return provider;
        }
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

    /**
     * Whether this webapp uses the short-edges cutout mode, where {@link
     * org.chromium.components.browser_ui.display_cutout.DisplayCutoutController} owns the
     * edge-to-edge state instead of the activity. See {@link
     * BaseCustomTabActivity#isShortEdgesCutoutModeEnabledForDisplayMode} for the display modes this
     * applies to.
     */
    private boolean isShortEdgesCutoutModeEnabledForApp() {
        BrowserServicesIntentDataProvider provider = getIntentDataProvider();
        if (provider == null) {
            if (!mBuiltStartupIntentDataProvider) {
                // The color scheme is unused when parsing a webapp intent, and the night mode
                // controller this activity would read it from does not exist this early.
                mStartupIntent = getIntent();
                mStartupIntentDataProvider =
                        buildIntentDataProvider(
                                mStartupIntent, CustomTabsIntent.COLOR_SCHEME_LIGHT);
                mBuiltStartupIntentDataProvider = true;
            }
            provider = mStartupIntentDataProvider;
        }
        return provider != null
                && isShortEdgesCutoutModeEnabledForDisplayMode(provider.getResolvedDisplayMode());
    }

    // In short-edges cutout mode, intentionally skip the activity-level edge-to-edge token at
    // creation time and let DisplayCutoutController acquire it later, only after the page declares
    // viewport-fit=cover. Drawing edge-to-edge unconditionally on create would push webapps under
    // the status bar even when the page never opted in.
    @Override
    protected boolean shouldDrawEdgeToEdgeOnCreate() {
        return !isShortEdgesCutoutModeEnabledForApp() && super.shouldDrawEdgeToEdgeOnCreate();
    }

    @Override
    protected boolean canColorStatusBarWithEdgeToEdgeHelper() {
        return isShortEdgesCutoutModeEnabledForApp()
                || super.canColorStatusBarWithEdgeToEdgeHelper();
    }

    @Override
    protected boolean canSetTransparentStatusBarWithoutDelegate() {
        return isShortEdgesCutoutModeEnabledForApp();
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
        if (BaseCustomTabActivity.isWindowInitiallyTranslucent(this)) {
            return null;
        }
        BrowserServicesIntentDataProvider intentDataProvider = getIntentDataProvider();
        if (intentDataProvider != null) {
            WebappInfo webappInfo = WebappInfo.create(intentDataProvider);
            if (webappInfo != null && intentDataProvider.getWebappExtras() != null) {
                int backgroundColor =
                        ColorUtils.getOpaqueColor(webappInfo.backgroundColorFallbackToDefault());
                return new ColorDrawable(backgroundColor);
            }
        }
        return super.getBackgroundDrawable();
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
