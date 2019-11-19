// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.graphics.PixelFormat;
import android.os.Build;
import android.os.SystemClock;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewPropertyAnimator;
import android.view.ViewTreeObserver;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.CachedMetrics;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.CompositorView;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.reflect.Method;

import javax.inject.Inject;

/** Shows and hides splash screen for Webapps, WebAPKs and TWAs. */
public class SplashController extends EmptyTabObserver implements InflationObserver, Destroyable {
    private static class SingleShotOnDrawListener implements ViewTreeObserver.OnDrawListener {
        private final View mView;
        private final Runnable mAction;
        private boolean mHasRun;

        public static void install(View view, Runnable action) {
            view.getViewTreeObserver().addOnDrawListener(
                    new SingleShotOnDrawListener(view, action));
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

    // SplashHidesReason defined in tools/metrics/histograms/enums.xml.
    @IntDef({SplashHidesReason.PAINT, SplashHidesReason.LOAD_FINISHED,
            SplashHidesReason.LOAD_FAILED, SplashHidesReason.CRASH})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SplashHidesReason {
        int PAINT = 0;
        int LOAD_FINISHED = 1;
        int LOAD_FAILED = 2;
        int CRASH = 3;
        int NUM_ENTRIES = 4;
    }

    @IntDef({TranslucencyRemoval.NONE, TranslucencyRemoval.ON_SPLASH_SHOWN,
            TranslucencyRemoval.ON_SPLASH_HIDDEN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TranslucencyRemoval {
        int NONE = 0;
        int ON_SPLASH_SHOWN = 1;
        int ON_SPLASH_HIDDEN = 2;
    }

    private static final String TAG = "SplashController";

    private final Activity mActivity;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final TabObserverRegistrar mTabObserverRegistrar;

    private SplashDelegate mDelegate;

    /** View to which the splash screen is added. */
    private ViewGroup mParentView;

    @Nullable
    private View mSplashView;

    @Nullable
    private ViewPropertyAnimator mFadeOutAnimator;

    /** The duration of the splash hide animation. */
    private long mSplashHideAnimationDurationMs;

    /** Indicates when translucency should be removed. */
    private @TranslucencyRemoval int mTranslucencyRemovalStrategy;

    private boolean mDidPreInflationStartup;

    /** Whether the splash hide animation was started. */
    private boolean mWasSplashHideAnimationStarted;

    /** Time that the splash screen was shown. */
    private long mSplashShownTimestamp;

    /** Whether translucency was removed. */
    private boolean mRemovedTranslucency;

    private ObserverList<SplashscreenObserver> mObservers;

    @Inject
    public SplashController(Activity activity, ActivityLifecycleDispatcher lifecycleDispatcher,
            TabObserverRegistrar tabObserverRegistrar) {
        mActivity = activity;
        mLifecycleDispatcher = lifecycleDispatcher;
        mTabObserverRegistrar = tabObserverRegistrar;
        mObservers = new ObserverList<>();
        mTranslucencyRemovalStrategy = TranslucencyRemoval.NONE;

        mLifecycleDispatcher.register(this);
        mTabObserverRegistrar.registerTabObserver(this);
    }

    public void setConfig(SplashDelegate delegate, boolean isWindowInitiallyTranslucent,
            long splashHideAnimationDurationMs) {
        mDelegate = delegate;
        mTranslucencyRemovalStrategy =
                computeTranslucencyRemovalStrategy(isWindowInitiallyTranslucent);
        mSplashHideAnimationDurationMs = splashHideAnimationDurationMs;
        if (mDidPreInflationStartup) {
            showSplash();
        }
    }

    /**
     * Brings splash view back to the front of the parent's view hierarchy, reattaching the view to
     * the parent if necessary.
     */
    public void bringSplashBackToFront() {
        if (mSplashView == null) return;

        if (mSplashView.getParent() != null) {
            mParentView.removeView(mSplashView);
        }
        mParentView.addView(mSplashView);
    }

    public boolean isSplashShowing() {
        return mSplashView != null;
    }

    @VisibleForTesting
    View getSplashScreenForTests() {
        return mSplashView;
    }

    @VisibleForTesting
    public boolean wasSplashScreenHiddenForTests() {
        return mSplashShownTimestamp > 0 && mSplashView == null;
    }

    @Override
    public void onPreInflationStartup() {
        mDidPreInflationStartup = true;
        if (mDelegate != null) {
            showSplash();
        }
    }

    @Override
    public void onPostInflationStartup() {
        if (mTranslucencyRemovalStrategy == TranslucencyRemoval.ON_SPLASH_SHOWN) {
            // In rare cases I see toolbar flickering. TODO(pshmakov): investigate why.
            mActivity.findViewById(R.id.coordinator).setVisibility(View.INVISIBLE);
        }
    }

    @Override
    public void destroy() {
        if (mFadeOutAnimator != null) {
            mFadeOutAnimator.cancel();
        }
    }

    @Override
    public void didFirstVisuallyNonEmptyPaint(Tab tab) {
        if (canHideSplashScreen()) {
            hideSplash(tab, SplashHidesReason.PAINT);
        }
    }

    @Override
    public void onPageLoadFinished(Tab tab, String url) {
        if (canHideSplashScreen()) {
            hideSplash(tab, SplashHidesReason.LOAD_FINISHED);
        }
    }

    @Override
    public void onPageLoadFailed(Tab tab, int errorCode) {
        if (canHideSplashScreen()) {
            hideSplash(tab, SplashHidesReason.LOAD_FAILED);
        }
    }

    @Override
    public void onCrash(Tab tab) {
        hideSplash(tab, SplashHidesReason.CRASH);
    }

    private void showSplash() {
        mSplashShownTimestamp = SystemClock.elapsedRealtime();
        try (TraceEvent te = TraceEvent.scoped("SplashScreen.build")) {
            mSplashView = mDelegate.buildSplashView();
        }
        if (mSplashView == null) {
            mTabObserverRegistrar.unregisterTabObserver(this);
            mLifecycleDispatcher.unregister(this);
            if (mTranslucencyRemovalStrategy != TranslucencyRemoval.NONE) {
                removeTranslucency();
            }
            return;
        }

        mParentView = (ViewGroup) mActivity.findViewById(android.R.id.content);
        mParentView.addView(mSplashView);

        recordTraceEventsShowedSplash();

        if (mTranslucencyRemovalStrategy == TranslucencyRemoval.ON_SPLASH_SHOWN) {
            // Without swapping the pixel format, removing translucency is only safe before
            // SurfaceView is attached.
            removeTranslucency();
        }
    }

    private static @TranslucencyRemoval int computeTranslucencyRemovalStrategy(
            boolean isWindowInitiallyTranslucent) {
        if (!isWindowInitiallyTranslucent) return TranslucencyRemoval.NONE;

        // Activity#convertFromTranslucent() incorrectly makes the Window opaque when a surface view
        // is attached. This is fixed in http://b/126897750#comment14 The bug causes the SurfaceView
        // to become black. We need to manually swap the pixel format to restore it. When hardware
        // acceleration is disabled, swapping the pixel format causes the surface to get recreated.
        // A bug fix in Android N preserves the old surface till the new one is drawn.
        //
        // Removing tranlucency is important for performance, otherwise the windows under Chrome
        // will continue being drawn (e.g. launcher with wallpaper). Without removing translucency,
        // we also see visual glitches in the following cases:
        // - closing activity (example: https://crbug.com/856544#c41)
        // - send activity to the background (example: https://crbug.com/856544#c30)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
                && FeatureUtilities.isSwapPixelFormatToFixConvertFromTranslucentEnabled()) {
            return TranslucencyRemoval.ON_SPLASH_HIDDEN;
        }
        return TranslucencyRemoval.ON_SPLASH_SHOWN;
    }

    private boolean canHideSplashScreen() {
        return !mDelegate.shouldWaitForSubsequentPageLoadToHideSplash();
    }

    /** Hides the splash screen. */
    private void hideSplash(final Tab tab, final @SplashHidesReason int reason) {
        if (mTranslucencyRemovalStrategy == TranslucencyRemoval.ON_SPLASH_HIDDEN
                && !mRemovedTranslucency) {
            removeTranslucency();

            // Activity#convertFromTranslucent() incorrectly makes the Window opaque -
            // WindowStateAnimator#setOpaqueLocked(true) - when a surface view is
            // attached. This is fixed in http://b/126897750#comment14. The
            // Window currently has format PixelFormat.TRANSLUCENT (Set by the SurfaceView's
            // ViewParent#requestTransparentRegion() call). Swap the pixel format to
            // force an opacity change back to non-opaque.
            mActivity.getWindow().setFormat(PixelFormat.TRANSPARENT);

            mParentView.invalidate();
        }

        if (reason == SplashHidesReason.LOAD_FAILED || reason == SplashHidesReason.CRASH) {
            animateHideSplash(tab, reason);
            return;
        }
        // Delay hiding the splash screen till the compositor has finished drawing the next frame.
        // Without this callback we were seeing a short flash of white between the splash screen and
        // the web content (crbug.com/734500).
        CompositorView compositorView =
                tab.getActivity().getCompositorViewHolder().getCompositorView();
        compositorView.surfaceRedrawNeededAsync(() -> { animateHideSplash(tab, reason); });
    }

    private void removeTranslucency() {
        mRemovedTranslucency = true;

        // Removing the temporary translucency, so that underlying windows don't get drawn.
        try {
            Method method = Activity.class.getDeclaredMethod("convertFromTranslucent");
            method.setAccessible(true);
            method.invoke(mActivity);
        } catch (ReflectiveOperationException e) {
            // Method not found or threw an exception.
            CachedMetrics.BooleanHistogramSample histogram =
                    new CachedMetrics.BooleanHistogramSample(
                            "Mobile.Splash.TranslucencyRemovalFailed");
            histogram.record(true);
            assert false : "Failed to remove activity translucency reflectively";
            Log.e(TAG, "Failed to remove activity translucency reflectively");
        }

        notifyTranslucencyRemoved();
    }

    private void animateHideSplash(final Tab tab, final @SplashHidesReason int reason) {
        if (mWasSplashHideAnimationStarted) return;

        mWasSplashHideAnimationStarted = true;
        mTabObserverRegistrar.unregisterTabObserver(this);
        tab.removeObserver(this);

        recordTraceEventsStartedHidingSplash();

        // Show browser UI in case we hid it in onPostInflationStartup().
        mActivity.findViewById(R.id.coordinator).setVisibility(View.VISIBLE);

        if (mSplashHideAnimationDurationMs == 0) {
            hideSplashNow(tab, reason);
            return;
        }
        mFadeOutAnimator = mSplashView.animate()
                                   .alpha(0f)
                                   .setDuration(mSplashHideAnimationDurationMs)
                                   .withEndAction(() -> { hideSplashNow(tab, reason); });
    }

    private void hideSplashNow(Tab tab, @SplashHidesReason int reason) {
        mParentView.removeView(mSplashView);

        long splashHiddenTimestamp = SystemClock.elapsedRealtime();
        recordTraceEventsFinishedHidingSplash();

        assert mSplashShownTimestamp != 0;
        mDelegate.onSplashHidden(tab, reason, mSplashShownTimestamp, splashHiddenTimestamp);
        notifySplashscreenHidden(mSplashShownTimestamp, splashHiddenTimestamp);

        mLifecycleDispatcher.unregister(this);

        mDelegate = null;
        mSplashView = null;
        mFadeOutAnimator = null;
    }

    /**
     * Register an observer for the splashscreen hidden/visible events.
     */
    public void addObserver(SplashscreenObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Deregister an observer for the splashscreen hidden/visible events.
     */
    public void removeObserver(SplashscreenObserver observer) {
        mObservers.removeObserver(observer);
    }

    private void notifyTranslucencyRemoved() {
        for (SplashscreenObserver observer : mObservers) {
            observer.onTranslucencyRemoved();
        }
    }

    private void notifySplashscreenHidden(long startTimestamp, long endTimestmap) {
        for (SplashscreenObserver observer : mObservers) {
            observer.onSplashscreenHidden(startTimestamp, endTimestmap);
        }
        mObservers.clear();
    }

    private void recordTraceEventsShowedSplash() {
        SingleShotOnDrawListener.install(
                mParentView, () -> { TraceEvent.startAsync("SplashScreen.visible", hashCode()); });
    }

    private void recordTraceEventsStartedHidingSplash() {
        TraceEvent.startAsync("SplashScreen.hidingAnimation", hashCode());
    }

    private void recordTraceEventsFinishedHidingSplash() {
        TraceEvent.finishAsync("SplashScreen.hidingAnimation", hashCode());
        SingleShotOnDrawListener.install(mParentView,
                () -> { TraceEvent.finishAsync("WebappSplashScreen.visible", hashCode()); });
    }
}
