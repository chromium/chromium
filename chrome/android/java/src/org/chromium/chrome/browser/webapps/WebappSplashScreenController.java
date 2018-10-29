// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.FrameLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.metrics.WebApkUma;
import org.chromium.chrome.browser.metrics.WebappUma;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.webapps.WebappActivity.ActivityType;
import org.chromium.net.NetError;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.webapk.lib.common.splash.SplashLayout;

/** Shows and hides splash screen. */
class WebappSplashScreenController extends EmptyTabObserver {
    // No error.
    public static final int ERROR_OK = 0;

    /** Used to schedule splash screen hiding. */
    private CompositorViewHolder mCompositorViewHolder;

    /** View to which the splash screen is added. */
    private ViewGroup mParentView;

    /** Whether native was loaded. Native must be loaded in order to record metrics. */
    private boolean mNativeLoaded;

    /** Whether the splash screen layout was initialized. */
    private boolean mInitializedLayout;

    private ViewGroup mSplashScreen;
    private WebappUma mWebappUma;

    /** The error code of the navigation. */
    private int mErrorCode;

    private WebappOfflineDialog mOfflineDialog;

    /** Indicates whether reloading is allowed. */
    private boolean mAllowReloads;

    private String mAppName;

    private @ActivityType int mActivityType;

    private ObserverList<SplashscreenObserver> mObservers;

    public WebappSplashScreenController() {
        mWebappUma = new WebappUma();
        mObservers = new ObserverList<>();
        addObserver(mWebappUma);
    }

    /** Shows the splash screen. */
    public void showSplashScreen(
            @ActivityType int activityType, ViewGroup parentView, final WebappInfo webappInfo) {
        mActivityType = activityType;
        mParentView = parentView;
        mAppName = webappInfo.name();

        Context context = ContextUtils.getApplicationContext();
        final int backgroundColor = ColorUtils.getOpaqueColor(webappInfo.backgroundColor(
                ApiCompatibilityUtils.getColor(context.getResources(), R.color.webapp_default_bg)));

        mSplashScreen = new FrameLayout(context);
        mSplashScreen.setBackgroundColor(backgroundColor);
        mParentView.addView(mSplashScreen);
        startSplashscreenTraceEvents();

        notifySplashscreenVisible();
        mWebappUma.recordSplashscreenBackgroundColor(webappInfo.hasValidBackgroundColor()
                        ? WebappUma.SplashScreenColorStatus.CUSTOM
                        : WebappUma.SplashScreenColorStatus.DEFAULT);
        mWebappUma.recordSplashscreenThemeColor(webappInfo.hasValidThemeColor()
                        ? WebappUma.SplashScreenColorStatus.CUSTOM
                        : WebappUma.SplashScreenColorStatus.DEFAULT);

        if (activityType == WebappActivity.ActivityType.WEBAPK) {
            initializeLayout(webappInfo, backgroundColor, ((WebApkInfo) webappInfo).splashIcon());
            return;
        }

        WebappDataStorage storage =
                WebappRegistry.getInstance().getWebappDataStorage(webappInfo.id());
        if (storage == null) {
            initializeLayout(webappInfo, backgroundColor, null);
            return;
        }

        storage.getSplashScreenImage(new WebappDataStorage.FetchCallback<Bitmap>() {
            @Override
            public void onDataRetrieved(Bitmap splashImage) {
                initializeLayout(webappInfo, backgroundColor, splashImage);
            }
        });
    }

    /**
     * Transfers a {@param viewHierarchy} to the splashscreen's parent view while keeping the
     * splashscreen on top.
     */
    public void setViewHierarchyBelowSplashscreen(ViewGroup viewHierarchy) {
        WarmupManager.transferViewHeirarchy(viewHierarchy, mParentView);
        mParentView.bringChildToFront(mSplashScreen);
    }

    /** Should be called once native has loaded. */
    public void onFinishedNativeInit(Tab tab, CompositorViewHolder compositorViewHolder) {
        mNativeLoaded = true;
        mCompositorViewHolder = compositorViewHolder;
        tab.addObserver(this);
        if (mInitializedLayout) mWebappUma.commitMetrics();
    }

    @VisibleForTesting
    ViewGroup getSplashScreenForTests() {
        return mSplashScreen;
    }

    @Override
    public void didFirstVisuallyNonEmptyPaint(Tab tab) {
        if (canHideSplashScreen()) {
            hideSplashScreenOnDrawingFinished(tab, WebappUma.SplashScreenHidesReason.PAINT);
        }
    }

    @Override
    public void onPageLoadFinished(Tab tab) {
        if (canHideSplashScreen()) {
            hideSplashScreenOnDrawingFinished(tab, WebappUma.SplashScreenHidesReason.LOAD_FINISHED);
        }
    }

    @Override
    public void onPageLoadFailed(Tab tab, int errorCode) {
        if (canHideSplashScreen()) {
            animateHidingSplashScreen(tab, WebappUma.SplashScreenHidesReason.LOAD_FAILED);
        }
    }

    @Override
    public void onCrash(Tab tab) {
        animateHidingSplashScreen(tab, WebappUma.SplashScreenHidesReason.CRASH);
    }

    @Override
    public void onDidFinishNavigation(final Tab tab, final String url, boolean isInMainFrame,
            boolean isErrorPage, boolean hasCommitted, boolean isSameDocument,
            boolean isFragmentNavigation, Integer pageTransition, int errorCode,
            int httpStatusCode) {
        if (mActivityType == WebappActivity.ActivityType.WEBAPP || !isInMainFrame) return;

        mErrorCode = errorCode;
        switch (mErrorCode) {
            case ERROR_OK:
                if (mOfflineDialog != null) {
                    mOfflineDialog.cancel();
                    mOfflineDialog = null;
                }
                break;
            case NetError.ERR_NETWORK_CHANGED:
                onNetworkChanged(tab);
                break;
            default:
                onNetworkError(tab, errorCode);
                break;
        }
        WebApkUma.recordNetworkErrorWhenLaunch(-errorCode);
    }

    protected boolean canHideSplashScreen() {
        if (mActivityType == WebappActivity.ActivityType.WEBAPP) return true;
        return mErrorCode != NetError.ERR_INTERNET_DISCONNECTED
                && mErrorCode != NetError.ERR_NETWORK_CHANGED;
    }

    private void onNetworkChanged(Tab tab) {
        if (!mAllowReloads) return;

        // It is possible that we get {@link NetError.ERR_NETWORK_CHANGED} during the first
        // reload after the device is online. The navigation will fail until the next auto
        // reload fired by {@link NetErrorHelperCore}. We call reload explicitly to reduce the
        // waiting time.
        tab.reloadIgnoringCache();
        mAllowReloads = false;
    }

    private void onNetworkError(final Tab tab, int errorCode) {
        if (mOfflineDialog != null || tab.getActivity() == null) return;

        final NetworkChangeNotifier.ConnectionTypeObserver observer =
                new NetworkChangeNotifier.ConnectionTypeObserver() {
                    @Override
                    public void onConnectionTypeChanged(int connectionType) {
                        if (!NetworkChangeNotifier.isOnline()) return;

                        NetworkChangeNotifier.removeConnectionTypeObserver(this);
                        tab.reloadIgnoringCache();
                        // One more reload is allowed after the network connection is back.
                        mAllowReloads = true;
                    }
                };

        NetworkChangeNotifier.addConnectionTypeObserver(observer);
        mOfflineDialog = new WebappOfflineDialog();
        mOfflineDialog.show(tab.getActivity(), mAppName,
                mActivityType == WebappActivity.ActivityType.WEBAPK, errorCode);
    }

    /** Sets the splash screen layout and sets the splash screen's title and icon. */
    private void initializeLayout(WebappInfo webappInfo, int backgroundColor, Bitmap splashImage) {
        mInitializedLayout = true;
        Context context = ContextUtils.getApplicationContext();
        Resources resources = context.getResources();

        Bitmap displayIcon = splashImage;
        boolean displayIconGenerated = false;
        if (displayIcon == null) {
            displayIcon = webappInfo.icon();
            displayIconGenerated = webappInfo.isIconGenerated();
        }
        @SplashLayout.IconClassification
        int displayIconClassification =
                SplashLayout.classifyIcon(resources, displayIcon, displayIconGenerated);

        if (displayIconClassification == SplashLayout.IconClassification.INVALID) {
            mWebappUma.recordSplashscreenIconType(WebappUma.SplashScreenIconType.NONE);
        } else {
            // Record stats about the splash screen.
            @WebappUma.SplashScreenIconType
            int splashScreenIconType;
            if (splashImage == null) {
                splashScreenIconType = WebappUma.SplashScreenIconType.FALLBACK;
            } else if (displayIconClassification == SplashLayout.IconClassification.SMALL) {
                splashScreenIconType = WebappUma.SplashScreenIconType.CUSTOM_SMALL;
            } else {
                splashScreenIconType = WebappUma.SplashScreenIconType.CUSTOM;
            }
            mWebappUma.recordSplashscreenIconType(splashScreenIconType);
            mWebappUma.recordSplashscreenIconSize(
                    Math.round(displayIcon.getScaledWidth(resources.getDisplayMetrics())
                            / resources.getDisplayMetrics().density));
        }

        SplashLayout.createLayout(context, mSplashScreen, displayIcon, displayIconClassification,
                webappInfo.name(),
                ColorUtils.shouldUseLightForegroundOnBackground(backgroundColor));

        if (mNativeLoaded) mWebappUma.commitMetrics();
    }

    /**
     * Schedules the splash screen hiding once the compositor has finished drawing a frame.
     *
     * Without this callback we were seeing a short flash of white between the splash screen and
     * the web content (crbug.com/734500).
     * */
    private void hideSplashScreenOnDrawingFinished(final Tab tab, final int reason) {
        if (mSplashScreen == null) return;

        if (mCompositorViewHolder == null) {
            animateHidingSplashScreen(tab, reason);
            return;
        }

        mCompositorViewHolder.getCompositorView().surfaceRedrawNeededAsync(
                () -> { animateHidingSplashScreen(tab, reason); });
    }

    /** Performs the splash screen hiding animation. */
    private void animateHidingSplashScreen(final Tab tab, final int reason) {
        if (mSplashScreen == null) return;

        mSplashScreen.animate().alpha(0f).withEndAction(new Runnable() {
            @Override
            public void run() {
                if (mSplashScreen == null) return;
                mParentView.removeView(mSplashScreen);
                finishSplashscreenTraceEvents();
                tab.removeObserver(WebappSplashScreenController.this);
                mSplashScreen = null;
                mCompositorViewHolder = null;
                notifySplashscreenHidden(reason);
            }
        });
    }

    private static class SingleShotOnDrawListener implements ViewTreeObserver.OnDrawListener {
        private final View mView;
        private final Runnable mAction;
        private boolean mHasRun;

        public static void install(View view, Runnable action) {
            SingleShotOnDrawListener listener = new SingleShotOnDrawListener(view, action);
            view.getViewTreeObserver().addOnDrawListener(listener);
        }

        private SingleShotOnDrawListener(View view, Runnable action) {
            mView = view;
            mAction = action;
        }

        @Override
        public void onDraw() {
            if (mHasRun) return;
            mHasRun = true;
            mAction.run();
            // Cannot call removeOnDrawListener within OnDraw, so do on next tick.
            mView.post(() -> mView.getViewTreeObserver().removeOnDrawListener(this));
        }
    };

    private void startSplashscreenTraceEvents() {
        TraceEvent.startAsync("WebappSplashScreen", hashCode());
        SingleShotOnDrawListener.install(mParentView,
                () -> { TraceEvent.startAsync("WebappSplashScreen.visible", hashCode()); });
    }

    private void finishSplashscreenTraceEvents() {
        TraceEvent.finishAsync("WebappSplashScreen", hashCode());
        SingleShotOnDrawListener.install(mParentView,
                () -> { TraceEvent.finishAsync("WebappSplashScreen.visible", hashCode()); });
    }

    /**
     * Register an observer for the splashscreen hidden/visible events.
     */
    public void addObserver(SplashscreenObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Deegister an observer for the splashscreen hidden/visible events.
     */
    public void removeObserver(SplashscreenObserver observer) {
        mObservers.removeObserver(observer);
    }

    private void notifySplashscreenVisible() {
        for (SplashscreenObserver observer : mObservers) {
            observer.onSplashscreenShown();
        }
    }

    private void notifySplashscreenHidden(int reason) {
        for (SplashscreenObserver observer : mObservers) {
            observer.onSplashscreenHidden(reason);
        }
        mObservers.clear();
    }
}
