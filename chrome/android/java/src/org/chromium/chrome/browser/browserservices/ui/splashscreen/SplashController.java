// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.splashscreen;

import android.app.Activity;
import android.graphics.PixelFormat;
import android.os.SystemClock;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewPropertyAnimator;
import android.view.ViewTreeObserver;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TwaFinishHandler;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabOrientationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabCreationMode;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.reflect.Method;

import javax.inject.Inject;

import dagger.Lazy;

/** Shows and hides splash screen for Webapps, WebAPKs and TWAs. */
@ActivityScope
public class SplashController
        extends CustomTabTabObserver implements InflationObserver, DestroyObserver {
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

    @IntDef({TranslucencyRemoval.NONE, TranslucencyRemoval.ON_SPLASH_SHOWN,
            TranslucencyRemoval.ON_SPLASH_HIDDEN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TranslucencyRemoval {
        int NONE = 0;
        int ON_SPLASH_SHOWN = 1;
        int ON_SPLASH_HIDDEN = 2;
    }

    private final Activity mActivity;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final TabObserverRegistrar mTabObserverRegistrar;
    private final TwaFinishHandler mFinishHandler;
    private final CustomTabActivityTabProvider mTabProvider;
    private final Lazy<CompositorViewHolder> mCompositorViewHolder;

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
            TabObserverRegistrar tabObserverRegistrar,
            CustomTabOrientationController orientationController, TwaFinishHandler finishHandler,
            CustomTabActivityTabProvider tabProvider,
            Lazy<CompositorViewHolder> compositorViewHolder) {
        mActivity = activity;
        mLifecycleDispatcher = lifecycleDispatcher;
        mTabObserverRegistrar = tabObserverRegistrar;
        mObservers = new ObserverList<>();
        mTranslucencyRemovalStrategy = TranslucencyRemoval.NONE;
        mFinishHandler = finishHandler;
        mTabProvider = tabProvider;
        mCompositorViewHolder = compositorViewHolder;

        boolean isWindowInitiallyTranslucent =
                BaseCustomTabActivity.isWindowInitiallyTranslucent(activity);
        mTranslucencyRemovalStrategy =
                computeTranslucencyRemovalStrategy(isWindowInitiallyTranslucent);

        orientationController.delayOrientationRequestsIfNeeded(this, isWindowInitiallyTranslucent);

        mLifecycleDispatcher.register(this);
        mTabObserverRegistrar.registerActivityTabObserver(this);
    }

    public void setConfig(SplashDelegate delegate, long splashHideAnimationDurationMs) {
        mDelegate = delegate;
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

    @VisibleForTesting
    public View getSplashScreenForTests() {
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
        bringSplashBackToFront();
    }

    @Override
    public void onDestroy() {
        if (mFadeOutAnimator != null) {
            mFadeOutAnimator.cancel();
        }
    }

    @Override
    public void didFirstVisuallyNonEmptyPaint(Tab tab) {
        if (canHideSplashScreen()) {
            hideSplash(tab, false /* loadFailed */);
        }
    }

    @Override
    public void onPageLoadFinished(Tab tab, GURL url) {
        if (canHideSplashScreen()) {
            hideSplash(tab, false /* loadFailed */);
        }
    }

    @Override
    public void onPageLoadFailed(Tab tab, int errorCode) {
        if (canHideSplashScreen()) {
            hideSplash(tab, true /* loadFailed */);
        }
    }

    @Override
    public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
        if (!tab.isLoading() && isInteractable
                && mTabProvider.getInitialTabCreationMode() == TabCreationMode.RESTORED
                && canHideSplashScreen()) {
            hideSplash(tab, false /* loadFailed */);
        }
    }

    @Override
    public void onCrash(Tab tab) {
        hideSplash(tab, true /* loadFailed */);
    }

    private void showSplash() {
        mSplashShownTimestamp = SystemClock.elapsedRealtime();
        try (TraceEvent te = TraceEvent.scoped("SplashScreen.build")) {
            mSplashView = mDelegate.buildSplashView();
        }
        if (mSplashView == null) {
            mTabObserverRegistrar.unregisterActivityTabObserver(this);
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

        // If the client's activity is opaque, finishing the activities one after another may lead
        // to bottom activity showing itself in a short flash. The problem can be solved by bottom
        // activity killing the whole task.
        mFinishHandler.setShouldAttemptFinishingTask(true);
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
        // Removing translucency is important for performance, otherwise the windows under Chrome
        // will continue being drawn (e.g. launcher with wallpaper). Without removing translucency,
        // we also see visual glitches in the following cases:
        // - closing activity (example: https://crbug.com/856544#c41)
        // - send activity to the background (example: https://crbug.com/856544#c30)
        if (ChromeFeatureList.sSwapPixelFormatToFixConvertFromTranslucent.isEnabled()) {
            return TranslucencyRemoval.ON_SPLASH_HIDDEN;
        }
        return TranslucencyRemoval.ON_SPLASH_SHOWN;
    }

    private boolean canHideSplashScreen() {
        return !mDelegate.shouldWaitForSubsequentPageLoadToHideSplash();
    }

    /** Hides the splash screen. */
    private void hideSplash(final Tab tab, boolean loadFailed) {
        if (mLifecycleDispatcher.isActivityFinishingOrDestroyed()) {
            return;
        }

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

        if (loadFailed) {
            animateHideSplash(tab);
            return;
        }
        // Delay hiding the splash screen till the compositor has finished drawing the next frame.
        // Without this callback we were seeing a short flash of white between the splash screen and
        // the web content (crbug.com/734500).
        mCompositorViewHolder.get().getCompositorView().surfaceRedrawNeededAsync(
                () -> { animateHideSplash(tab); });
    }

    private void removeTranslucency() {
        mRemovedTranslucency = true;

        // Removing the temporary translucency, so that underlying windows don't get drawn.
        try {
            Method method = Activity.class.getDeclaredMethod("convertFromTranslucent");
            method.setAccessible(true);
            method.invoke(mActivity);
        } catch (ReflectiveOperationException e) {
        }

        notifyTranslucencyRemoved();
    }

    private void animateHideSplash(final Tab tab) {
        if (mWasSplashHideAnimationStarted) return;

        mWasSplashHideAnimationStarted = true;
        mTabObserverRegistrar.unregisterActivityTabObserver(this);

        recordTraceEventsStartedHidingSplash();

        // Show browser UI in case we hid it in onPostInflationStartup().
        mActivity.findViewById(R.id.coordinator).setVisibility(View.VISIBLE);

        if (mSplashHideAnimationDurationMs == 0) {
            hideSplashNow(tab);
            return;
        }
        mFadeOutAnimator = mSplashView.animate()
                                   .alpha(0f)
                                   .setDuration(mSplashHideAnimationDurationMs)
                                   .withEndAction(() -> { hideSplashNow(tab); });
    }

    private void hideSplashNow(Tab tab) {
        mParentView.removeView(mSplashView);

        long splashHiddenTimestamp = SystemClock.elapsedRealtime();
        recordTraceEventsFinishedHidingSplash();

        assert mSplashShownTimestamp != 0;
        mDelegate.onSplashHidden(tab, mSplashShownTimestamp, splashHiddenTimestamp);
        notifySplashscreenHidden(mSplashShownTimestamp, splashHiddenTimestamp);

        mFinishHandler.setShouldAttemptFinishingTask(false);

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
