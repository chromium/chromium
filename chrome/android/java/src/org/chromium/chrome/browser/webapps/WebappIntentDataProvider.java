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
import androidx.browser.customtabs.CustomTabsIntent;
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
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.device.mojom.ScreenOrientationLockType;

/**
 * Stores info about a web app.
 */
public class WebappIntentDataProvider extends BrowserServicesIntentDataProvider {
    private final Drawable mCloseButtonIcon;
    private final TrustedWebActivityDisplayMode mTwaDisplayMode;
    private final ShareData mShareData;
    private final @NonNull WebappExtras mWebappExtras;
    private final @Nullable WebApkExtras mWebApkExtras;
    private final @ActivityType int mActivityType;
    private final Intent mIntent;
    private final ColorProviderImpl mColorProvider;

    /**
     * Returns the toolbar color to use if a custom color is not specified by the webapp.
     */
    public static int getDefaultToolbarColor() {
        return Color.WHITE;
    }

    WebappIntentDataProvider(@NonNull Intent intent, int toolbarColor,
            boolean hasCustomToolbarColor, @Nullable ShareData shareData,
            @NonNull WebappExtras webappExtras, @Nullable WebApkExtras webApkExtras) {
        mIntent = intent;
        mColorProvider = new ColorProviderImpl(toolbarColor, hasCustomToolbarColor);
        final Context context = new ContextThemeWrapper(
                ContextUtils.getApplicationContext(), ActivityUtils.getThemeId());
        mCloseButtonIcon = TintedDrawable.constructTintedDrawable(context, R.drawable.btn_close);
        mTwaDisplayMode = (webappExtras.displayMode == DisplayMode.FULLSCREEN)
                ? new ImmersiveMode(false /* sticky */, LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT)
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
    @Nullable
    public Intent getIntent() {
        return mIntent;
    }

    @Override
    @Nullable
    public String getClientPackageName() {
        if (mWebApkExtras != null) {
            return mWebApkExtras.webApkPackageName;
        }
        return null;
    }

    @Override
    @Nullable
    public String getUrlToLoad() {
        return mWebappExtras.url;
    }

    @Override
    @NonNull
    public ColorProvider getColorProvider() {
        return mColorProvider;
    }

    @Override
    public Drawable getCloseButtonDrawable() {
        return mCloseButtonIcon;
    }

    @Override
    public int getTitleVisibilityState() {
        return CustomTabsIntent.SHOW_PAGE_TITLE;
    }

    @Override
    public boolean shouldShowShareMenuItem() {
        return true;
    }

    @Override
    @CustomTabsUiType
    public int getUiType() {
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
    public TrustedWebActivityDisplayMode getTwaDisplayMode() {
        return mTwaDisplayMode;
    }

    @Override
    @Nullable
    public ShareData getShareData() {
        return mShareData;
    }

    @Override
    @Nullable
    public WebappExtras getWebappExtras() {
        return mWebappExtras;
    }

    @Override
    @Nullable
    public WebApkExtras getWebApkExtras() {
        return mWebApkExtras;
    }

    @Override
    public @ScreenOrientationLockType.EnumType int getDefaultOrientation() {
        return mWebappExtras.orientation;
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
        @Nullable
        public Integer getNavigationBarColor() {
            return null;
        }

        @Override
        @Nullable
        public Integer getNavigationBarDividerColor() {
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
