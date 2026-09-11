// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.splashscreen.webapps;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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

/** Displays the splash screen for homescreen shortcuts and WebAPKs. */
@NullMarked
public class WebappSplashController implements SplashDelegate {
    public static final int HIDE_ANIMATION_DURATION_MS = 300;

    private final Activity mActivity;
    private final SplashController mSplashController;
    private final TabObserverRegistrar mTabObserverRegistrar;
    private final WebappInfo mWebappInfo;

    private @Nullable WebApkSplashNetworkErrorObserver mWebApkNetworkErrorObserver;

    public WebappSplashController(
            Activity activity,
            SplashController splashController,
            TabObserverRegistrar tabObserverRegistrar,
            BrowserServicesIntentDataProvider intentDataProvider) {
        mActivity = activity;
        mSplashController = splashController;
        mTabObserverRegistrar = tabObserverRegistrar;

        mWebappInfo = WebappInfo.create(intentDataProvider);

        mSplashController.setConfigAndShowSplash(this, HIDE_ANIMATION_DURATION_MS);

        if (mWebappInfo.isForWebApk()) {
            mWebApkNetworkErrorObserver = new WebApkSplashNetworkErrorObserver(activity);
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
        setupSplashInsets(splashScreen);

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
                (@Nullable Bitmap splashImage) ->
                        initializeWebApkInfoSplashLayout(
                                splashScreen, backgroundColor, splashImage, false));
        return splashScreen;
    }

    private void initializeWebApkInfoSplashLayout(
            ViewGroup splashScreen,
            int backgroundColor,
            @Nullable Bitmap splashImage,
            boolean isSplashIconMaskable) {
        Context context = ContextUtils.getApplicationContext();

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

        String packageName = mWebappInfo.webApkPackageName();
        assert packageName != null;
        Bitmap splashBitmap =
                FileUtils.queryBitmapFromContentProvider(
                        appContext,
                        Uri.parse(WebApkCommonUtils.generateSplashContentProviderUri(packageName)));
        if (splashBitmap != null) {
            splashView.setScaleType(ImageView.ScaleType.FIT_CENTER);
            splashView.setImageBitmap(splashBitmap);

            // Pad by system bar insets so the WebAPK splash icon aligns with where
            // SplashActivity displayed it without shifting.
            setupSplashInsets(splashView);
        }

        return splashView;
    }

    /**
     * Insets the splash view by system bar / caption bar insets.
     *
     * <p>WebAPK's SplashActivity only screenshots its content view (excluding caption / system
     * bars). Since Chrome runs edge-to-edge across the entire window, we add padding matching the
     * insets so the splash icon remains in the same position without shifting.
     */
    private void setupSplashInsets(View splashView) {
        ViewCompat.setOnApplyWindowInsetsListener(
                splashView,
                (v, insetsCompat) -> {
                    applySystemBarInsetsPadding(splashView, insetsCompat);
                    return insetsCompat;
                });

        applySystemBarInsetsPadding(splashView);

        splashView.addOnAttachStateChangeListener(
                new View.OnAttachStateChangeListener() {
                    @Override
                    public void onViewAttachedToWindow(View v) {
                        applySystemBarInsetsPadding(splashView);
                        ViewCompat.requestApplyInsets(splashView);
                    }

                    @Override
                    public void onViewDetachedFromWindow(View v) {}
                });
    }

    /** Resolves and applies system bar insets to the splash view padding. */
    private boolean applySystemBarInsetsPadding(View splashView) {
        WindowInsetsCompat rootInsets = ViewCompat.getRootWindowInsets(splashView);
        if (rootInsets == null && mActivity.getWindow().peekDecorView() != null) {
            rootInsets = ViewCompat.getRootWindowInsets(mActivity.getWindow().peekDecorView());
        }
        if (rootInsets != null) {
            return applySystemBarInsetsPadding(splashView, rootInsets);
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            WindowInsets windowInsets =
                    mActivity.getWindowManager().getCurrentWindowMetrics().getWindowInsets();
            return applySystemBarInsetsPadding(
                    splashView, WindowInsetsCompat.toWindowInsetsCompat(windowInsets, splashView));
        }
        return false;
    }

    private boolean applySystemBarInsetsPadding(View splashView, WindowInsetsCompat insetsCompat) {
        int insetTypes =
                WindowInsetsCompat.Type.systemBars()
                        | WindowInsetsCompat.Type.captionBar()
                        | WindowInsetsCompat.Type.mandatorySystemGestures();
        Insets insets = insetsCompat.getInsets(insetTypes);
        boolean changed =
                insets.top != splashView.getPaddingTop()
                        || insets.left != splashView.getPaddingLeft()
                        || insets.right != splashView.getPaddingRight()
                        || insets.bottom != splashView.getPaddingBottom();
        if (changed) {
            splashView.setPadding(insets.left, insets.top, insets.right, insets.bottom);
        }
        return changed;
    }
}
