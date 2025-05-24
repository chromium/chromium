// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;

import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.trusted.TrustedWebActivityDisplayMode;
import androidx.browser.trusted.TrustedWebActivityDisplayMode.DefaultMode;
import androidx.browser.trusted.TrustedWebActivityDisplayMode.ImmersiveMode;
import androidx.browser.trusted.sharing.ShareData;

import org.chromium.base.ContextUtils;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.ui.web_app_header.WebAppHeaderUtils;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.ui.util.ColorUtils;

/** Stores info about a web app. */
public class WebappIntentDataProvider extends BrowserServicesIntentDataProvider {
    private final Drawable mCloseButtonIcon;
    private final TrustedWebActivityDisplayMode mTwaDisplayMode;
    private final ShareData mShareData;
    private final @NonNull WebappExtras mWebappExtras;
    private final @Nullable WebApkExtras mWebApkExtras;
    private final @ActivityType int mActivityType;
    private final Intent mIntent;
    private final ColorProviderImpl mColorProvider;
    private final ColorProviderImpl mDarkColorProvider;
    private @DisplayMode.EnumType int mResolvedDisplayMode = DisplayMode.UNDEFINED;

    /** Returns the toolbar color to use if a custom color is not specified by the webapp. */
    public static int getDefaultToolbarColor() {
        return Color.WHITE;
    }

    /** Returns the toolbar color to use if a custom dark color is not specified by the webapp. */
    public static int getDefaultDarkToolbarColor() {
        return Color.BLACK;
    }

    WebappIntentDataProvider(
            @NonNull Intent intent,
            int toolbarColor,
            boolean hasCustomToolbarColor,
            int darkToolbarColor,
            boolean hasCustomDarkToolbarColor,
            @Nullable ShareData shareData,
            @NonNull WebappExtras webappExtras,
            @Nullable WebApkExtras webApkExtras) {
        mIntent = intent;
        mColorProvider = new ColorProviderImpl(toolbarColor, hasCustomToolbarColor);
        mDarkColorProvider = new ColorProviderImpl(darkToolbarColor, hasCustomDarkToolbarColor);
        final Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), ActivityUtils.getThemeId());
        mCloseButtonIcon = TintedDrawable.constructTintedDrawable(context, R.drawable.btn_close);
        mTwaDisplayMode =
                (webappExtras.displayMode == DisplayMode.FULLSCREEN)
                        ? new ImmersiveMode(
                                /* sticky= */ false, LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT)
                        : new DefaultMode();
        mShareData = shareData;
        mWebappExtras = webappExtras;
        mWebApkExtras = webApkExtras;
        mActivityType = (webApkExtras != null) ? ActivityType.WEB_APK : ActivityType.WEBAPP;
    }

    @Override
    public @ActivityType int getActivityType() {
        return mActivityType;
    }

    @Override
    public @Nullable Intent getIntent() {
        return mIntent;
    }

    @Override
    public @Nullable String getClientPackageName() {
        if (mWebApkExtras != null) {
            return mWebApkExtras.webApkPackageName;
        }
        return null;
    }

    @Override
    public String getUrlToLoad() {
        return mWebappExtras.url;
    }

    @Override
    public @NonNull ColorProvider getColorProvider() {
        boolean inDarkMode = ColorUtils.inNightMode(ContextUtils.getApplicationContext());
        boolean hasValidDarkToolbar = mDarkColorProvider.hasCustomToolbarColor();
        boolean hasValidLightToolbar = mColorProvider.hasCustomToolbarColor();
        return inDarkMode && (hasValidDarkToolbar || !hasValidLightToolbar)
                ? mDarkColorProvider
                : mColorProvider;
    }

    @Override
    public @NonNull ColorProvider getLightColorProvider() {
        return mColorProvider;
    }

    @Override
    public @NonNull ColorProvider getDarkColorProvider() {
        return mDarkColorProvider;
    }

    @Override
    public Drawable getCloseButtonDrawable() {
        return mCloseButtonIcon;
    }

    @Override
    public @TitleVisibility int getTitleVisibilityState() {
        return TitleVisibility.VISIBLE;
    }

    @Override
    public boolean shouldShowShareMenuItem() {
        return true;
    }

    @Override
    public @CustomTabsUiType int getUiType() {
        return CustomTabsUiType.MINIMAL_UI_WEBAPP;
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
    public TrustedWebActivityDisplayMode getProvidedTwaDisplayMode() {
        return mTwaDisplayMode;
    }

    @Override
    public @Nullable ShareData getShareData() {
        return mShareData;
    }

    @Override
    public @Nullable WebappExtras getWebappExtras() {
        return mWebappExtras;
    }

    @Override
    public @Nullable WebApkExtras getWebApkExtras() {
        return mWebApkExtras;
    }

    @Override
    public @ScreenOrientationLockType.EnumType int getDefaultOrientation() {
        return mWebappExtras.orientation;
    }

    @Override
    public int getResolvedDisplayMode() {
        if (mResolvedDisplayMode != DisplayMode.UNDEFINED) {
            return mResolvedDisplayMode;
        }

        mResolvedDisplayMode = resolveDisplayMode();
        return mResolvedDisplayMode;
    }

    private @DisplayMode.EnumType int resolveDisplayMode() {
        if (mWebappExtras.displayMode == DisplayMode.BROWSER) {
            // `browser` display mode web apps are not installable by default, because by the spec
            // they should be opened in a new tab or browser window, but in Chrome they can be
            // forcefully installed via app menu. In this case display mode should resolve to the
            // first supported display mode in the "fullscreen -> standalone -> minimal-ui ->
            // browser" fallback chain.
            if (WebAppHeaderUtils.isMinimalUiFlagEnabled()) {
                return DisplayMode.MINIMAL_UI;
            } else {
                return DisplayMode.STANDALONE;
            }
        }

        return mWebappExtras.displayMode;
    }

    private static final class ColorProviderImpl implements ColorProvider {
        private final int mToolbarColor;
        private final boolean mHasCustomToolbarColor;

        ColorProviderImpl(int toolbarColor, boolean hasCustomToolbarColor) {
            mToolbarColor = toolbarColor;
            mHasCustomToolbarColor = hasCustomToolbarColor;
        }

        @Override
        public int getToolbarColor() {
            return mToolbarColor;
        }

        @Override
        public boolean hasCustomToolbarColor() {
            return mHasCustomToolbarColor;
        }

        @Override
        public @Nullable Integer getNavigationBarColor() {
            return null;
        }

        @Override
        public @Nullable Integer getNavigationBarDividerColor() {
            return null;
        }

        @Override
        public int getBottomBarColor() {
            return getToolbarColor();
        }

        @Override
        public int getInitialBackgroundColor() {
            return Color.TRANSPARENT;
        }
    }
}
