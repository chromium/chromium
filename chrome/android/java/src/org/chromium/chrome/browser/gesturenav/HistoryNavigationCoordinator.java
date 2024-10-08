// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.graphics.Insets;
import android.os.Build;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.browser_ui.widget.TouchEventProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.base.BackGestureEventSwipeEdge;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator object for gesture navigation. */
public class HistoryNavigationCoordinator
        implements InsetObserver.WindowInsetObserver, PauseResumeWithNativeObserver {
    private final Runnable mUpdateNavigationStateRunnable = this::onNavigationStateChanged;

    private ViewGroup mParentView;
    private HistoryNavigationLayout mNavigationLayout;
    private InsetObserver mInsetObserver;
    private CurrentTabObserver mCurrentTabObserver;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private BackActionDelegate mBackActionDelegate;
    private Tab mTab;
    private boolean mEnabled;

    private NavigationHandler mNavigationHandler;

    private Supplier<TouchEventProvider> mTouchEventProvider;

    private boolean mForceFeatureEnabledForTesting;

    /**
     * Creates the coordinator for gesture navigation and initializes internal objects.
     *
     * @param window Window object.
     * @param lifecycleDispatcher Lifecycle dispatcher for the associated activity.
     * @param parentView Parent view of the gesture navigation layout.
     * @param requestRunnable Runnable executing the renderer update.
     * @param tabSupplier Activity tab supplier.
     * @param insetObserver View that provides information about the inset and inset capabilities of
     *     the device.
     * @param backActionDelegate Delegate handling actions for back gesture.
     * @param touchEventProvider {@link TouchEventProvider} object.
     * @return HistoryNavigationCoordinator object or null if not enabled via feature flag.
     */
    public static HistoryNavigationCoordinator create(
            WindowAndroid window,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            ViewGroup parentView,
            Runnable requestRunnable,
            ObservableSupplier<Tab> tabSupplier,
            InsetObserver insetObserver,
            BackActionDelegate backActionDelegate,
            Supplier<TouchEventProvider> touchEventProvider) {
        HistoryNavigationCoordinator coordinator = new HistoryNavigationCoordinator();
        coordinator.init(
                window,
                lifecycleDispatcher,
                parentView,
                requestRunnable,
                tabSupplier,
                insetObserver,
                backActionDelegate,
                touchEventProvider);
        return coordinator;
    }

    /** Initializes the navigation layout and internal objects. */
    private void init(
            WindowAndroid window,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            ViewGroup parentView,
            Runnable requestRunnable,
            ObservableSupplier<Tab> tabSupplier,
            InsetObserver insetObserver,
            BackActionDelegate backActionDelegate,
            Supplier<TouchEventProvider> touchEventProvider) {
        mNavigationLayout =
                new HistoryNavigationLayout(
                        parentView.getContext(),
                        (direction) -> mNavigationHandler.navigate(direction));

        mParentView = parentView;
        mActivityLifecycleDispatcher = lifecycleDispatcher;
        mBackActionDelegate = backActionDelegate;
        mTouchEventProvider = touchEventProvider;
        lifecycleDispatcher.register(this);

        // TODO(crbug.com/40770763): Look into enforcing the z-order of the views.
        parentView.addView(mNavigationLayout);

        mCurrentTabObserver =
                new CurrentTabObserver(
                        tabSupplier,
                        new EmptyTabObserver() {
                            @Override
                            public void onContentChanged(Tab tab) {
                                updateNavigationHandler();
                            }

                            @Override
                            public void onDestroyed(Tab tab) {
                                mTab = null;
                                updateNavigationHandler();
                            }
                        },
                        (tab) -> {
                            mTab = tab;
                            updateNavigationHandler();
                        });
        // We wouldn't hear about the first tab until the content changed or we switched tabs
        // if tabProvider.get() != null. Do here what we do when tab switching happens.
        // Otherwise, just initialize |mEnabled| in preparation of the initialization of
        // NavigationHandler for later tab switching/init.
        if (tabSupplier.get() != null) {
            mTab = tabSupplier.get();
            onNavigationStateChanged();
        } else {
            mEnabled = isFeatureEnabled();
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            mInsetObserver = insetObserver;
            insetObserver.addObserver(this);
        }
        GestureNavMetrics.logGestureType(isFeatureEnabled());
    }

    /** @return {@link TouchEventObserver} for gesture navigation component. */
    public @Nullable TouchEventObserver getTouchEventObserver() {
        // Can be null if gesture navigation was not triggered at all or already destroyed.
        return mNavigationHandler;
    }

    private static boolean isDetached(Tab tab) {
        return tab == null
                || tab.getWebContents() == null
                || tab.getWebContents().getTopLevelNativeWindow() == null;
    }

    /**
     * @return {@code} true if the feature is enabled.
     */
    private boolean isFeatureEnabled() {
        if (mForceFeatureEnabledForTesting) {
            return true;
        }

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return true;
        } else {
            // Preserve the previous enabled status if queried when the view is in detached state.
            if (mParentView == null || !mParentView.isAttachedToWindow()) return mEnabled;
            Insets insets = mParentView.getRootWindowInsets().getSystemGestureInsets();
            return insets.left == 0 && insets.right == 0;
        }
    }

    @Override
    public void onInsetChanged(int left, int top, int right, int bottom) {
        onNavigationStateChanged();
    }

    /**
     * Called when an event that can change the state of navigation feature. Update enabled status
     * and (re)initialize NavigationHandler if necessary.
     */
    private void onNavigationStateChanged() {
        boolean oldEnabled = mEnabled;
        mEnabled = isFeatureEnabled();
        if (mEnabled != oldEnabled) updateNavigationHandler();
    }

    /** Initialize or reset {@link NavigationHandler} using the enabled state. */
    private void updateNavigationHandler() {
        // Check against |mActivityLifecycleDisptacher|/|mTouchEventProvider| prevents the flow
        // after the destruction.
        if (!mEnabled
                || mActivityLifecycleDispatcher == null
                || mTouchEventProvider.get() == null) {
            return;
        }

        WebContents webContents = mTab != null ? mTab.getWebContents() : null;

        // Also updates NavigationHandler when tab == null (going into TabSwitcher).
        if (mTab == null || webContents != null) {
            if (mNavigationHandler == null) initNavigationHandler();
            mNavigationHandler.setTab(isDetached(mTab) ? null : mTab);
        }
    }

    /** Initialize {@link NavigationHandler} object. */
    private void initNavigationHandler() {
        PropertyModel model =
                new PropertyModel.Builder(GestureNavigationProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                model, mNavigationLayout, GestureNavigationViewBinder::bind);
        mNavigationHandler =
                new NavigationHandler(
                        model,
                        mNavigationLayout,
                        mBackActionDelegate,
                        mNavigationLayout::willNavigate);
        mTouchEventProvider.get().addTouchEventObserver(mNavigationHandler);
    }

    @Override
    public void onResumeWithNative() {
        // Check the enabled status again since the system gesture settings might have changed.
        // Post the task to work around wrong gesture insets returned from the framework.
        mParentView.post(mUpdateNavigationStateRunnable);
    }

    @Override
    public void onPauseWithNative() {}

    /** Starts preparing an edge swipe gesture. */
    public void startGesture() {
        // Simulates the initial onDown event to update the internal state.
        if (mNavigationHandler != null) mNavigationHandler.onDown();
    }

    /**
     * Makes UI visible when an edge swipe is made big enough to trigger it.
     *
     * @param initiatingEdge The edge of the screen from which navigation UI is being initiated.
     * @return {@code true} if history navigation is possible, even if there are no further session
     *     history entries in the given direction.
     */
    public boolean triggerUi(@BackGestureEventSwipeEdge int initiatingEdge) {
        return mNavigationHandler != null
                && mNavigationHandler.triggerUi(
                        initiatingEdge, NavigationHandler.TriggerUiCallSource.WEBPAGE_OVERSCROLL);
    }

    /**
     * Processes a motion event releasing the finger off the screen and possibly initializing the
     * navigation.
     *
     * @param allowNav {@code true} if release action is supposed to trigger navigation.
     */
    public void release(boolean allowNav) {
        if (mNavigationHandler != null) mNavigationHandler.release(allowNav);
    }

    /** Resets a gesture as the result of the successful navigation or cancellation. */
    public void reset() {
        if (mNavigationHandler != null) mNavigationHandler.reset();
    }

    /**
     * Signals a pull update.
     *
     * @param xDelta The change in horizontal pull distance (positive if toward right, negative if
     * left).
     * @param yDelta The change in vertical pull distance.
     */
    public void pull(float xDelta, float yDelta) {
        if (mNavigationHandler != null) {
            mNavigationHandler.pull(xDelta, yDelta);
        }
    }

    /** Destroy HistoryNavigationCoordinator object. */
    public void destroy() {
        if (mCurrentTabObserver != null) {
            mCurrentTabObserver.destroy();
            mCurrentTabObserver = null;
        }
        if (mInsetObserver != null) {
            mInsetObserver.removeObserver(this);
            mInsetObserver = null;
        }
        mNavigationLayout = null;
        mParentView.removeCallbacks(mUpdateNavigationStateRunnable);

        if (mNavigationHandler != null) {
            mNavigationHandler.setTab(null);
            mNavigationHandler.destroy();
            if (mTouchEventProvider.get() != null) {
                mTouchEventProvider.get().removeTouchEventObserver(mNavigationHandler);
            }
            mNavigationHandler = null;
        }
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcher = null;
        }
    }

    NavigationHandler getNavigationHandlerForTesting() {
        return mNavigationHandler;
    }

    HistoryNavigationLayout getLayoutForTesting() {
        return mNavigationLayout;
    }

    void forceFeatureEnabledForTesting() {
        mForceFeatureEnabledForTesting = true;
        onNavigationStateChanged();
    }
}
