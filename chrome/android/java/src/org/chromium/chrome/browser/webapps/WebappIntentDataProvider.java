// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.Drawable;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.trusted.sharing.ShareData;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.components.browser_ui.widget.TintedDrawable;

/**
 * Stores info about a web app.
 */
public class WebappIntentDataProvider extends BrowserServicesIntentDataProvider {
    private final int mToolbarColor;
    private final boolean mHasCustomToolbarColor;
    private final Drawable mCloseButtonIcon;
    private final ShareData mShareData;
    private final @NonNull WebappExtras mWebappExtras;
    private final @Nullable WebApkExtras mWebApkExtras;
    private final @ActivityType int mActivityType;
    private final Intent mIntent;

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
        mToolbarColor = toolbarColor;
        mHasCustomToolbarColor = hasCustomToolbarColor;
        mCloseButtonIcon = TintedDrawable.constructTintedDrawable(
                ContextUtils.getApplicationContext(), R.drawable.btn_close);
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
    public int getToolbarColor() {
        return mToolbarColor;
    }

    @Override
    public boolean hasCustomToolbarColor() {
        return mHasCustomToolbarColor;
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
    public int getDefaultOrientation() {
        return mWebappExtras.orientation;
    }
}
