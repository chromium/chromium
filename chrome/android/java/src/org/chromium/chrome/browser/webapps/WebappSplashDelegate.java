// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.net.Uri;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.webapk.lib.common.WebApkCommonUtils;
import org.chromium.webapk.lib.common.splash.SplashLayout;

/** Delegate for splash screen for webapps and WebAPKs. */
public class WebappSplashDelegate implements SplashDelegate {
    public static final int HIDE_ANIMATION_DURATION_MS = 300;
    public static final String HISTOGRAM_SPLASHSCREEN_HIDES = "Webapp.Splashscreen.Hides";

    private TabObserverRegistrar mTabObserverRegistrar;

    private WebappInfo mWebappInfo;

    private WebApkSplashNetworkErrorObserver mWebApkNetworkErrorObserver;

    public WebappSplashDelegate(
            Activity activity, TabObserverRegistrar tabObserverRegistrar, WebappInfo webappInfo) {
        mTabObserverRegistrar = tabObserverRegistrar;
        mWebappInfo = webappInfo;

        if (mWebappInfo.isForWebApk()) {
            mWebApkNetworkErrorObserver =
                    new WebApkSplashNetworkErrorObserver(activity, mWebappInfo.name());
            mTabObserverRegistrar.registerTabObserver(mWebApkNetworkErrorObserver);
        }
    }

    @Override
    public View buildSplashView() {
        Context appContext = ContextUtils.getApplicationContext();
        int backgroundColor =
                ColorUtils.getOpaqueColor(mWebappInfo.backgroundColorFallbackToDefault());
        if (mWebappInfo.isSplashProvidedByWebApk()) {
            return buildSplashWithWebApkProvidedScreenshot(appContext, backgroundColor);
        }
        return buildSplashFromWebApkInfo(appContext, backgroundColor);
    }

    @Override
    public void onSplashHidden(Tab tab, @SplashController.SplashHidesReason int reason,
            long startTimestamp, long endTimestamp) {
        if (mWebApkNetworkErrorObserver != null) {
            mTabObserverRegistrar.unregisterTabObserver(mWebApkNetworkErrorObserver);
            tab.removeObserver(mWebApkNetworkErrorObserver);
            mWebApkNetworkErrorObserver = null;
        }

        RecordHistogram.recordEnumeratedHistogram(HISTOGRAM_SPLASHSCREEN_HIDES, reason,
                SplashController.SplashHidesReason.NUM_ENTRIES);
    }

    @Override
    public boolean shouldWaitForSubsequentPageLoadToHideSplash() {
        return mWebApkNetworkErrorObserver != null
                && mWebApkNetworkErrorObserver.isNetworkErrorDialogVisible();
    }

    /** Builds splash screen from WebApkInfo. */
    private View buildSplashFromWebApkInfo(Context appContext, int backgroundColor) {
        ViewGroup splashScreen = new FrameLayout(appContext);
        splashScreen.setBackgroundColor(backgroundColor);

        if (mWebappInfo.isForWebApk()) {
            WebApkInfo webApkInfo = (WebApkInfo) mWebappInfo;
            initializeWebApkInfoSplashLayout(splashScreen, backgroundColor,
                    webApkInfo.splashIcon().bitmap(), webApkInfo.isSplashIconMaskable());
            return splashScreen;
        }

        WebappDataStorage storage =
                WebappRegistry.getInstance().getWebappDataStorage(mWebappInfo.id());
        if (storage == null) {
            initializeWebApkInfoSplashLayout(splashScreen, backgroundColor, null, false);
            return splashScreen;
        }

        storage.getSplashScreenImage(new WebappDataStorage.FetchCallback<Bitmap>() {
            @Override
            public void onDataRetrieved(Bitmap splashImage) {
                initializeWebApkInfoSplashLayout(splashScreen, backgroundColor, splashImage, false);
            }
        });
        return splashScreen;
    }

    private void initializeWebApkInfoSplashLayout(ViewGroup splashScreen, int backgroundColor,
            Bitmap splashImage, boolean isSplashIconMaskable) {
        Context context = ContextUtils.getApplicationContext();
        Resources resources = context.getResources();

        Bitmap selectedIcon = splashImage;
        boolean selectedIconGenerated = false;
        boolean selectedIconAdaptive = isSplashIconMaskable;
        if (selectedIcon == null) {
            selectedIcon = mWebappInfo.icon().bitmap();
            selectedIconGenerated = mWebappInfo.isIconGenerated();
            selectedIconAdaptive = mWebappInfo.isIconAdaptive();
        }
        SplashLayout.createLayout(context, splashScreen, selectedIcon, selectedIconAdaptive,
                selectedIconGenerated, mWebappInfo.name(),
                ColorUtils.shouldUseLightForegroundOnBackground(backgroundColor));
    }

    /** Builds splash screen using screenshot provided by WebAPK. */
    private View buildSplashWithWebApkProvidedScreenshot(Context appContext, int backgroundColor) {
        ImageView splashView = new ImageView(appContext);
        splashView.setBackgroundColor(backgroundColor);

        Bitmap splashBitmap = null;
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            splashBitmap = FileUtils.queryBitmapFromContentProvider(appContext,
                    Uri.parse(WebApkCommonUtils.generateSplashContentProviderUri(
                            mWebappInfo.webApkPackageName())));
        }
        if (splashBitmap != null) {
            splashView.setScaleType(ImageView.ScaleType.FIT_CENTER);
            splashView.setImageBitmap(splashBitmap);
        }

        return splashView;
    }
}
