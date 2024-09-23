// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.splashscreen.webapps;

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
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.SplashController;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.SplashDelegate;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.WebApkSplashNetworkErrorObserver;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.ui.util.ColorUtils;
import org.chromium.webapk.lib.common.WebApkCommonUtils;
import org.chromium.webapk.lib.common.splash.SplashLayout;

import javax.inject.Inject;

/** Displays the splash screen for homescreen shortcuts and WebAPKs. */
public class WebappSplashController implements SplashDelegate {
    public static final int HIDE_ANIMATION_DURATION_MS = 300;

    private SplashController mSplashController;
    private TabObserverRegistrar mTabObserverRegistrar;
    private WebappInfo mWebappInfo;

    private WebApkSplashNetworkErrorObserver mWebApkNetworkErrorObserver;

    @Inject
    public WebappSplashController(
            SplashController splashController,
            Activity activity,
            TabObserverRegistrar tabObserverRegistrar,
            BrowserServicesIntentDataProvider intentDataProvider) {
        mSplashController = splashController;
        mTabObserverRegistrar = tabObserverRegistrar;
        mWebappInfo = WebappInfo.create(intentDataProvider);

        mSplashController.setConfig(this, HIDE_ANIMATION_DURATION_MS);

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
    public void onSplashHidden(Tab tab, long startTimestamp, long endTimestamp) {
        if (mWebApkNetworkErrorObserver != null) {
            mTabObserverRegistrar.unregisterTabObserver(mWebApkNetworkErrorObserver);
            tab.removeObserver(mWebApkNetworkErrorObserver);
            mWebApkNetworkErrorObserver = null;
        }
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
            initializeWebApkInfoSplashLayout(
                    splashScreen,
                    backgroundColor,
                    mWebappInfo.splashIcon().bitmap(),
                    mWebappInfo.isSplashIconMaskable());
            return splashScreen;
        }

        WebappDataStorage storage =
                WebappRegistry.getInstance().getWebappDataStorage(mWebappInfo.id());
        if (storage == null) {
            initializeWebApkInfoSplashLayout(splashScreen, backgroundColor, null, false);
            return splashScreen;
        }

        storage.getSplashScreenImage(
                new WebappDataStorage.FetchCallback<Bitmap>() {
                    @Override
                    public void onDataRetrieved(Bitmap splashImage) {
                        initializeWebApkInfoSplashLayout(
                                splashScreen, backgroundColor, splashImage, false);
                    }
                });
        return splashScreen;
    }

    private void initializeWebApkInfoSplashLayout(
            ViewGroup splashScreen,
            int backgroundColor,
            Bitmap splashImage,
            boolean isSplashIconMaskable) {
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
        SplashLayout.createLayout(
                context,
                splashScreen,
                selectedIcon,
                selectedIconAdaptive,
                selectedIconGenerated,
                mWebappInfo.name(),
                ColorUtils.shouldUseLightForegroundOnBackground(backgroundColor));
    }

    /** Builds splash screen using screenshot provided by WebAPK. */
    private View buildSplashWithWebApkProvidedScreenshot(Context appContext, int backgroundColor) {
        ImageView splashView = new ImageView(appContext);
        splashView.setBackgroundColor(backgroundColor);

        Bitmap splashBitmap =
                FileUtils.queryBitmapFromContentProvider(
                        appContext,
                        Uri.parse(
                                WebApkCommonUtils.generateSplashContentProviderUri(
                                        mWebappInfo.webApkPackageName())));
        if (splashBitmap != null) {
            splashView.setScaleType(ImageView.ScaleType.FIT_CENTER);
            splashView.setImageBitmap(splashBitmap);
        }

        return splashView;
    }
}
