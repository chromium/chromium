// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.auth.AuthTabIntent;
import androidx.browser.auth.AuthTabSessionToken;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
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
public class AuthTabIntentDataProvider extends BrowserServicesIntentDataProvider {
    private final @NonNull Intent mIntent;
    private final @Nullable String mClientPackageName;
    private final SessionHolder<AuthTabSessionToken> mSession;
    private final @NonNull ColorProvider mColorProvider;
    private final @NonNull Drawable mCloseButtonIcon;
    private final @Nullable String mRedirectScheme;
    private final @Nullable String mRedirectHost;
    private final @Nullable String mRedirectPath;
    private final @CustomTabProfileType int mCustomTabMode;

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
     * @param colorScheme The color scheme the Auth Tab should use.
     */
    public AuthTabIntentDataProvider(
            Intent intent, Context context, @CustomTabsIntent.ColorScheme int colorScheme) {
        assert intent != null;
        mIntent = intent;
        AuthTabSessionToken token = AuthTabSessionToken.createSessionTokenFromIntent(intent);
        mSession = token != null ? new SessionHolder<>(token) : null;
        mClientPackageName =
                IntentUtils.safeGetStringExtra(
                        intent, IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE);
        mColorProvider = new AuthTabColorProvider(intent, context, colorScheme);
        mCloseButtonIcon = retrieveCloseButtonIcon(intent, context);
        // TODO(crbug.com/353586171): We should disallow http/https and other known schemes such as
        // content://, file://, chrome:// etc. Can be handled using methods in UrlUtilities, but we
        // might want to disallow more.
        mRedirectScheme =
                IntentUtils.safeGetStringExtra(intent, AuthTabIntent.EXTRA_REDIRECT_SCHEME);
        boolean httpsEnabled = ChromeFeatureList.sCctAuthTabEnableHttpsRedirects.isEnabled();
        String host =
                httpsEnabled
                        ? IntentUtils.safeGetStringExtra(
                                intent, AuthTabIntent.EXTRA_HTTPS_REDIRECT_HOST)
                        : null;
        String path =
                httpsEnabled
                        ? IntentUtils.safeGetStringExtra(
                                intent, AuthTabIntent.EXTRA_HTTPS_REDIRECT_PATH)
                        : null;
        GURL redirectUrl = new GURL(UrlConstants.HTTPS_URL_PREFIX + host + path);
        mRedirectHost = redirectUrl.getHost();
        mRedirectPath = redirectUrl.getPath();
        mCustomTabMode =
                isEphemeralTab(intent)
                        ? CustomTabProfileType.EPHEMERAL
                        : CustomTabProfileType.REGULAR;

        logFeatureUsage(intent, colorScheme);
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

    @Nullable
    @Override
    public SessionHolder<AuthTabSessionToken> getSession() {
        return mSession;
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
    public @TitleVisibility int getTitleVisibilityState() {
        return TitleVisibility.VISIBLE;
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
        return mCustomTabMode;
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
    private void logFeatureUsage(Intent intent, @CustomTabsIntent.ColorScheme int colorScheme) {
        CustomTabsFeatureUsage featureUsage = new CustomTabsFeatureUsage();

        // Ordering: Log all the features ordered by enum, when they apply.
        if (IntentUtils.safeHasExtra(intent, CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON)) {
            featureUsage.log(CustomTabsFeatureUsage.CustomTabsFeature.EXTRA_CLOSE_BUTTON_ICON);
        }
        if (colorScheme == CustomTabsIntent.COLOR_SCHEME_DARK) {
            featureUsage.log(CustomTabsFeatureUsage.CustomTabsFeature.CTF_DARK);
        }
        if (colorScheme == CustomTabsIntent.COLOR_SCHEME_LIGHT) {
            featureUsage.log(CustomTabsFeatureUsage.CustomTabsFeature.CTF_LIGHT);
        }
        if (IntentUtils.safeHasExtra(intent, CustomTabsIntent.EXTRA_COLOR_SCHEME)) {
            featureUsage.log(CustomTabsFeatureUsage.CustomTabsFeature.EXTRA_COLOR_SCHEME);
        }
        if (colorScheme == CustomTabsIntent.COLOR_SCHEME_SYSTEM) {
            featureUsage.log(CustomTabsFeatureUsage.CustomTabsFeature.CTF_SYSTEM);
        }
        if (mCustomTabMode == CustomTabProfileType.EPHEMERAL) {
            featureUsage.log(
                    CustomTabsFeatureUsage.CustomTabsFeature.EXTRA_ENABLE_EPHEMERAL_BROWSING);
        }
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

    private static Drawable retrieveCloseButtonIcon(Intent intent, Context context) {
        Bitmap bitmap =
                IntentUtils.safeGetParcelableExtra(
                        intent, CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON);
        if (bitmap == null) {
            return TintedDrawable.constructTintedDrawable(context, R.drawable.btn_close);
        }

        int size = context.getResources().getDimensionPixelSize(R.dimen.toolbar_icon_height);
        if (bitmap.getWidth() == size && bitmap.getHeight() == size) {
            return new TintedDrawable(context, bitmap);
        }

        Bitmap scaledBitmap = Bitmap.createScaledBitmap(bitmap, size, size, true);
        bitmap.recycle();
        return new TintedDrawable(context, scaledBitmap);
    }

    private static boolean isEphemeralTab(Intent intent) {
        if (!ChromeFeatureList.sCctEphemeralMode.isEnabled()) return false;
        return IntentUtils.safeGetBooleanExtra(
                intent, CustomTabsIntent.EXTRA_ENABLE_EPHEMERAL_BROWSING, false);
    }
}
