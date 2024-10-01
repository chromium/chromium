// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.OptIn;
import androidx.browser.auth.AuthTabIntent;
import androidx.browser.auth.ExperimentalAuthTab;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

/**
 * A model class that parses the incoming intent for Auth Tab specific data.
 *
 * <p>Lifecycle: is activity-scoped, i.e. one instance per CustomTabActivity instance. Must be
 * re-created when color scheme changes, which happens automatically since color scheme change leads
 * to activity re-creation.
 */
@OptIn(markerClass = ExperimentalAuthTab.class)
public class AuthTabIntentDataProvider extends BrowserServicesIntentDataProvider {
    // TODO(b/358167556): Move these to AndroidX AuthTabIntent.
    public static final String EXTRA_HTTPS_REDIRECT_HOST =
            "androidx.browser.auth.extra.HTTPS_REDIRECT_HOST";
    public static final String EXTRA_HTTPS_REDIRECT_PATH =
            "androidx.browser.auth.extra.HTTPS_REDIRECT_PATH";

    private final @NonNull Intent mIntent;
    private final @Nullable String mClientPackageName;
    private final @NonNull ColorProvider mColorProvider;
    private final @NonNull Drawable mCloseButtonIcon;
    private final @Nullable String mRedirectScheme;
    private final @Nullable String mRedirectHost;
    private final @Nullable String mRedirectPath;

    @Nullable private String mUrlToLoad;

    public static boolean isAuthTabIntent(Intent intent) {
        if (!ChromeFeatureList.sCctAuthTab.isEnabled()) return false;
        return IntentUtils.safeGetBooleanExtra(intent, AuthTabIntent.EXTRA_LAUNCH_AUTH_TAB, false);
    }

    /**
     * Constructs an {@link AuthTabIntentDataProvider}.
     *
     * @param intent The {@link Intent} to launch the Auth Tab.
     * @param context The {@link Context}.
     */
    public AuthTabIntentDataProvider(Intent intent, Context context) {
        assert intent != null;
        mIntent = intent;
        mClientPackageName =
                IntentUtils.safeGetStringExtra(
                        intent, IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE);
        mColorProvider = new AuthTabColorProvider(context);
        mCloseButtonIcon = TintedDrawable.constructTintedDrawable(context, R.drawable.btn_close);
        // TODO(crbug.com/353586171): We should disallow http/https and other known schemes such as
        // content://, file://, chrome:// etc. Can be handled using methods in UrlUtilities, but we
        // might want to disallow more.
        mRedirectScheme =
                IntentUtils.safeGetStringExtra(intent, AuthTabIntent.EXTRA_REDIRECT_SCHEME);
        String host = IntentUtils.safeGetStringExtra(intent, EXTRA_HTTPS_REDIRECT_HOST);
        String path = IntentUtils.safeGetStringExtra(intent, EXTRA_HTTPS_REDIRECT_PATH);
        GURL redirectUrl = new GURL(UrlConstants.HTTPS_URL_PREFIX + host + path);
        mRedirectHost = redirectUrl.getHost();
        mRedirectPath = redirectUrl.getPath();

        logFeatureUsage();
    }

    @Override
    public String getAuthRedirectHost() {
        return mRedirectHost;
    }

    @Override
    public String getAuthRedirectPath() {
        return mRedirectPath;
    }

    @Override
    public @ActivityType int getActivityType() {
        return ActivityType.AUTH_TAB;
    }

    @Override
    public Intent getIntent() {
        return mIntent;
    }

    @Override
    public String getClientPackageName() {
        return mClientPackageName;
    }

    @Override
    public String getUrlToLoad() {
        if (mUrlToLoad == null) {
            mUrlToLoad = IntentHandler.getUrlFromIntent(mIntent);
        }
        return mUrlToLoad;
    }

    @Override
    public boolean shouldEnableUrlBarHiding() {
        return false;
    }

    @Override
    public ColorProvider getColorProvider() {
        return mColorProvider;
    }

    @Nullable
    @Override
    public Drawable getCloseButtonDrawable() {
        return mCloseButtonIcon;
    }

    @Override
    public int getTitleVisibilityState() {
        return CustomTabsIntent.SHOW_PAGE_TITLE;
    }

    @Override
    public @CustomTabsUiType int getUiType() {
        return CustomTabsUiType.AUTH_TAB;
    }

    @Override
    public boolean shouldShowStarButton() {
        return false;
    }

    @Override
    public boolean shouldShowDownloadButton() {
        return false;
    }

    @Override
    public @CustomTabProfileType int getCustomTabMode() {
        // TODO(crbug.com/359315737): Handle properly once the relevant code lands in AndroidX.
        return CustomTabProfileType.REGULAR;
    }

    @Override
    public boolean isAuthTab() {
        return true;
    }

    @Override
    public String getAuthRedirectScheme() {
        return mRedirectScheme;
    }

    /**
     * Logs the usage of Auth Tab features to a large enum histogram in order to track usage by
     * apps.
     */
    private void logFeatureUsage() {
        if (!CustomTabsFeatureUsage.isEnabled()) return;
        CustomTabsFeatureUsage featureUsage = new CustomTabsFeatureUsage();

        // Ordering: Log all the features ordered by enum, when they apply.
        featureUsage.log(CustomTabsFeatureUsage.CustomTabsFeature.EXTRA_LAUNCH_AUTH_TAB);
        if (mRedirectScheme != null) {
            featureUsage.log(CustomTabsFeatureUsage.CustomTabsFeature.EXTRA_REDIRECT_SCHEME);
        }
        if (mRedirectHost != null) {
            featureUsage.log(CustomTabsFeatureUsage.CustomTabsFeature.EXTRA_HTTPS_REDIRECT_HOST);
        }
        if (mRedirectPath != null) {
            featureUsage.log(CustomTabsFeatureUsage.CustomTabsFeature.EXTRA_HTTPS_REDIRECT_PATH);
        }
    }
}
