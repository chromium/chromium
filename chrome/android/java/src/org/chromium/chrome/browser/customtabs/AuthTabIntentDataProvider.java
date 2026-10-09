// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;

import androidx.annotation.VisibleForTesting;
import androidx.browser.auth.AuthTabIntent;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.browserservices.intents.CustomTabIntentDataHolder;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.CustomTabProfileType;
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
@NullMarked
public class AuthTabIntentDataProvider extends BrowserServicesIntentDataProvider {
    private final Intent mIntent;
    private final CustomTabIntentDataHolder mDataHolder;
    private final ColorProvider mColorProvider;
    private final Drawable mCloseButtonIcon;

    private @Nullable String mUrlToLoad;

    public static boolean isAuthTabIntent(Intent intent) {
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
        this(intent, context, colorScheme, /* dataHolder= */ null);
    }

    /**
     * Constructs an {@link AuthTabIntentDataProvider}.
     *
     * @param intent The {@link Intent} to launch the Auth Tab.
     * @param context The {@link Context}.
     * @param colorScheme The color scheme the Auth Tab should use.
     * @param dataHolder Data holder used to recover intent data from the saved instance state. A
     *     null value should be passed if there is no saved instance state.
     */
    public AuthTabIntentDataProvider(
            Intent intent,
            Context context,
            @CustomTabsIntent.ColorScheme int colorScheme,
            @Nullable CustomTabIntentDataHolder dataHolder) {
        assert intent != null;
        mIntent = intent;

        if (dataHolder != null) {
            mDataHolder = dataHolder;
        } else {
            var builder = new CustomTabIntentDataHolder.Builder();
            builder.setActivityType(ActivityType.AUTH_TAB);
            builder.setUiType(CustomTabsUiType.AUTH_TAB);
            builder.setTitleVisibilityState(TitleVisibility.VISIBLE);
            builder.setDisableStar(true);
            builder.setDisableDownload(true);

            var session = SessionHolder.getSessionHolderFromIntent(intent);
            builder.setSessionHolder(session);
            builder.setClientPackageName(
                    IntentUtils.safeGetStringExtra(
                            intent, IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE));

            // TODO(crbug.com/353586171): We should disallow http/https and other known schemes
            // such as content://, file://, chrome:// etc. Can be handled using methods in
            // UrlUtilities, but we might want to disallow more.
            builder.setAuthRedirectScheme(
                    IntentUtils.safeGetStringExtra(intent, AuthTabIntent.EXTRA_REDIRECT_SCHEME));
            String host =
                    IntentUtils.safeGetStringExtra(intent, AuthTabIntent.EXTRA_HTTPS_REDIRECT_HOST);
            String path =
                    IntentUtils.safeGetStringExtra(intent, AuthTabIntent.EXTRA_HTTPS_REDIRECT_PATH);
            GURL redirectUrl = new GURL(UrlConstants.HTTPS_URL_PREFIX + host + path);
            builder.setAuthRedirectHost(redirectUrl.getHost());
            builder.setAuthRedirectPath(redirectUrl.getPath());
            builder.setCustomTabMode(
                    isEphemeralTab(intent)
                            ? CustomTabProfileType.EPHEMERAL
                            : CustomTabProfileType.REGULAR);
            builder.setIsCloseButtonEnabled(true);
            builder.setCloseButtonIcon(retrieveCloseButtonIconBitmap(intent, context));

            mDataHolder = builder.build();
        }

        mColorProvider = new AuthTabColorProvider(intent, context, colorScheme);
        mCloseButtonIcon =
                mDataHolder.mCloseButtonIcon != null
                        ? new TintedDrawable(context, mDataHolder.mCloseButtonIcon)
                        : TintedDrawable.constructTintedDrawable(context, R.drawable.btn_close);

        logFeatureUsage(intent, colorScheme);
    }

    @Override
    public @Nullable String getAuthRedirectHost() {
        return mDataHolder.mAuthRedirectHost;
    }

    @Override
    public @Nullable String getAuthRedirectPath() {
        return mDataHolder.mAuthRedirectPath;
    }

    @Override
    public @ActivityType int getActivityType() {
        assert mDataHolder.mActivityType == ActivityType.AUTH_TAB;
        return mDataHolder.mActivityType;
    }

    @Override
    public Intent getIntent() {
        return mIntent;
    }

    @Override
    public @Nullable SessionHolder getSession() {
        return mDataHolder.mSessionHolder;
    }

    @Override
    public @Nullable String getClientPackageName() {
        return mDataHolder.mClientPackageName;
    }

    @Override
    public @Nullable String getUrlToLoad() {
        if (mUrlToLoad == null) {
            mUrlToLoad = IntentHandler.getUrlFromIntent(mIntent);
        }
        return mUrlToLoad;
    }

    @Override
    public boolean shouldEnableUrlBarHiding() {
        assert !mDataHolder.mEnableUrlBarHiding;
        return mDataHolder.mEnableUrlBarHiding;
    }

    @Override
    public ColorProvider getColorProvider() {
        return mColorProvider;
    }

    @Override
    public boolean isCloseButtonEnabled() {
        assert mDataHolder.mIsCloseButtonEnabled;
        return mDataHolder.mIsCloseButtonEnabled;
    }

    @Override
    public @Nullable Drawable getCloseButtonDrawable() {
        return mCloseButtonIcon;
    }

    @Override
    public @TitleVisibility int getTitleVisibilityState() {
        assert mDataHolder.mTitleVisibilityState == TitleVisibility.VISIBLE;
        return mDataHolder.mTitleVisibilityState;
    }

    @Override
    public @CustomTabsUiType int getUiType() {
        assert mDataHolder.mUiType == CustomTabsUiType.AUTH_TAB;
        return mDataHolder.mUiType;
    }

    @Override
    public boolean shouldShowStarButton() {
        assert mDataHolder.mDisableStar;
        return !mDataHolder.mDisableStar;
    }

    @Override
    public boolean shouldShowDownloadButton() {
        assert mDataHolder.mDisableDownload;
        return !mDataHolder.mDisableDownload;
    }

    @Override
    public @CustomTabProfileType int getCustomTabMode() {
        return mDataHolder.mCustomTabMode;
    }

    @Override
    public boolean isAuthTab() {
        return true;
    }

    @Override
    public @Nullable String getAuthRedirectScheme() {
        return mDataHolder.mAuthRedirectScheme;
    }

    @Override
    public int getFeatureIdForMetricsCollection() {
        if (mDataHolder.mCustomTabMode == CustomTabProfileType.EPHEMERAL) {
            return IncognitoCctCallerId.EPHEMERAL_TAB;
        }

        return super.getFeatureIdForMetricsCollection();
    }

    /**
     * Logs the usage of Auth Tab features to a large enum histogram in order to track usage by
     * apps.
     */
    private void logFeatureUsage(Intent intent, @CustomTabsIntent.ColorScheme int colorScheme) {
        CustomTabsFeatureUsage featureUsage = new CustomTabsFeatureUsage();

        // Ordering: Log all the features ordered by enum, when they apply.
        if (mDataHolder.mCloseButtonIcon != null) {
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
        if (mDataHolder.mCustomTabMode == CustomTabProfileType.EPHEMERAL) {
            featureUsage.log(
                    CustomTabsFeatureUsage.CustomTabsFeature.EXTRA_ENABLE_EPHEMERAL_BROWSING);
        }
        featureUsage.log(CustomTabsFeatureUsage.CustomTabsFeature.EXTRA_LAUNCH_AUTH_TAB);
        if (mDataHolder.mAuthRedirectScheme != null) {
            featureUsage.log(CustomTabsFeatureUsage.CustomTabsFeature.EXTRA_REDIRECT_SCHEME);
        }
        if (mDataHolder.mAuthRedirectHost != null) {
            featureUsage.log(CustomTabsFeatureUsage.CustomTabsFeature.EXTRA_HTTPS_REDIRECT_HOST);
        }
        if (mDataHolder.mAuthRedirectPath != null) {
            featureUsage.log(CustomTabsFeatureUsage.CustomTabsFeature.EXTRA_HTTPS_REDIRECT_PATH);
        }
    }

    private static @Nullable Bitmap retrieveCloseButtonIconBitmap(Intent intent, Context context) {
        Bitmap bitmap =
                IntentUtils.safeGetParcelableExtra(
                        intent, CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON);
        if (bitmap == null) return null;

        int size = context.getResources().getDimensionPixelSize(R.dimen.toolbar_icon_height);
        if (bitmap.getWidth() == size && bitmap.getHeight() == size) {
            return bitmap;
        }

        Bitmap scaledBitmap = Bitmap.createScaledBitmap(bitmap, size, size, true);
        bitmap.recycle();
        return scaledBitmap;
    }

    @VisibleForTesting
    static boolean isEphemeralTab(Intent intent) {
        return IntentUtils.safeGetBooleanExtra(
                intent, CustomTabsIntent.EXTRA_ENABLE_EPHEMERAL_BROWSING, false);
    }

    @Override
    public CustomTabIntentDataHolder getCustomTabIntentDataHolder() {
        return mDataHolder;
    }
}
