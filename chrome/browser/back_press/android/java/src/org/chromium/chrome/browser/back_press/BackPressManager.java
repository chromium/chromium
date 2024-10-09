// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import android.annotation.SuppressLint;
import android.util.SparseIntArray;

import androidx.activity.BackEventCompat;
import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.Type;
import org.chromium.components.cached_flags.BooleanCachedFieldTrialParameter;

import java.util.ArrayList;
import java.util.List;

/**
 * A central manager class to handle the back gesture. Every component/feature which is going to
 * intercept the back press event must implement the {@link BackPressHandler} and be registered
 * in a proper order.
 * In order to register a Handler:
 * 1. Implement {@link BackPressHandler}.
 * 2. Add a new {@link Type} which implies the order of intercepting.
 * 3. Add a new value in {@link #sMetricsMap} which stands for the histograms.
 * 4. Call {@link #addHandler(BackPressHandler, int)} to register the implementer of
 * {@link BackPressHandler} with the new defined {@link Type}.
 */
public class BackPressManager implements Destroyable {
    public static final BooleanCachedFieldTrialParameter TAB_HISTORY_RECOVER =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.BACK_GESTURE_REFACTOR, "tab_history_recover", false);
    private static final SparseIntArray sMetricsMap;
    private static final int sMetricsMaxValue;

    static {
        // Max value is 23 - 1 obsolete value +1 for 0 indexing = 22 elements.
        SparseIntArray map = new SparseIntArray(20);
        map.put(Type.TEXT_BUBBLE, 0);
        // map.put(Type.VR_DELEGATE, 1);
        // map.put(Type.AR_DELEGATE, 2);
        map.put(Type.SCENE_OVERLAY, 3);
        map.put(Type.START_SURFACE, 4);
        map.put(Type.SELECTION_POPUP, 5);
        map.put(Type.MANUAL_FILLING, 6);
        map.put(Type.FULLSCREEN, 7);
        map.put(Type.BOTTOM_SHEET, 8);
        map.put(Type.TAB_MODAL_HANDLER, 9);
        map.put(Type.TAB_SWITCHER, 10);
        map.put(Type.CLOSE_WATCHER, 11);
        map.put(Type.TAB_HISTORY, 12);
        // map.put(Type.TAB_RETURN_TO_CHROME_START_SURFACE, 13);
        map.put(Type.SHOW_READING_LIST, 14);
        map.put(Type.MINIMIZE_APP_AND_CLOSE_TAB, 15);
        map.put(Type.FIND_TOOLBAR, 16);
        map.put(Type.LOCATION_BAR, 17);
        map.put(Type.XR_DELEGATE, 18);
        map.put(Type.BOTTOM_CONTROLS, 20);
        map.put(Type.HUB, 21);
        map.put(Type.ARCHIVED_TABS_DIALOG, 22);

        // Add new one here and update array size.
        sMetricsMaxValue = 23;
        sMetricsMap = map;
    }

    private final OnBackPressedCallback mCallback =
            new OnBackPressedCallback(false) {
                private BackPressHandler mActiveHandler;
                private BackEventCompat mLastBackEvent;

                @SuppressLint("WrongConstant") // Suppress mLastCalledHandlerType assignment warning
                @Override
                public void handleOnBackPressed() {
                    if (mOnBackPressed != null) mOnBackPressed.run();
                    recordSystemBackCountIfBeforeFirstVisibleContent();
                    mLastCalledHandlerType = -1;
                    if (ChromeFeatureList.sLockBackPressHandlerAtStart.isEnabled()
                            && mActiveHandler != null) {
                        Boolean enabled = mActiveHandler.getHandleBackPressChangedSupplier().get();
                        if (enabled != null && enabled) {
                            int result = mActiveHandler.handleBackPress();
                            int index = BackPressManager.this.getIndex(mActiveHandler);
                            mLastCalledHandlerType = index;
                            if (result == BackPressResult.FAILURE) {
                                BackPressManager.this.handleBackPress();
                            } else {
                                record(index);
                            }
                        } else {
                            BackPressManager.this.handleBackPress();
                        }
                    } else {
                        BackPressManager.this.handleBackPress();
                    }

                    // This means this back is triggered by a gesture rather than the back button.
                    if (mLastBackEvent != null
                            && mLastCalledHandlerType != -1
                            && mIsGestureNavEnabledSupplier.get()) {
                        BackPressMetrics.recordBackPressFromEdge(
                                mLastCalledHandlerType, mLastBackEvent.getSwipeEdge());

                        if (mLastCalledHandlerType == Type.TAB_HISTORY) {
                            BackPressMetrics.recordTabNavigationSwipedFromEdge(
                                    mLastBackEvent.getSwipeEdge());
                        }
                    }

                    mActiveHandler = null;
                    mLastBackEvent = null;
                }

                // Following methods are only triggered on API 34+.
                @Override
                public void handleOnBackStarted(@NonNull BackEventCompat backEvent) {
                    mActiveHandler = getEnabledBackPressHandler();
                    assert mActiveHandler != null;
                    mActiveHandler.handleOnBackStarted(backEvent);
                    mLastBackEvent = backEvent;
                }

                @Override
                public void handleOnBackCancelled() {
                    if (mActiveHandler == null) return;
                    mActiveHandler.handleOnBackCancelled();
                    mActiveHandler = null;
                    mLastBackEvent = null;
                }

                @Override
                public void handleOnBackProgressed(@NonNull BackEventCompat backEvent) {
                    if (mActiveHandler == null) return;
                    mActiveHandler.handleOnBackProgressed(backEvent);
                }
            };

    static final String HISTOGRAM = "Android.BackPress.Intercept";
    static final String HISTOGRAM_CUSTOM_TAB_SAME_TASK =
            "Android.BackPress.Intercept.CustomTab.SameTask";
    static final String HISTOGRAM_CUSTOM_TAB_SEPARATE_TASK =
            "Android.BackPress.Intercept.CustomTab.SeparateTask";
    static final String FAILURE_HISTOGRAM = "Android.BackPress.Failure";

    private final BackPressHandler[] mHandlers = new BackPressHandler[Type.NUM_TYPES];
    private final boolean mUseSystemBack;
    private boolean mHasSystemBackArm;

    private final Callback<Boolean>[] mObserverCallbacks = new Callback[Type.NUM_TYPES];
    private Runnable mFallbackOnBackPressed;
    private int mLastCalledHandlerType = -1;
    private boolean mBackBeforeFirstVisibleContentRecorded;
    private Supplier<Boolean> mIsFirstVisibleContentDrawnSupplier;
    private Runnable mOnBackPressed;
    private Supplier<Boolean> mIsGestureNavEnabledSupplier = () -> false;

    /**
     * @return True if the back gesture refactor is enabled.
     */
    public static boolean isEnabled() {
        return ChromeFeatureList.sBackGestureRefactorAndroid.isEnabled();
    }

    /**
     * @return True if ActivityTabProvider should replace ChromeTabActivity#getActivityTab
     */
    public static boolean shouldUseActivityTabProvider() {
        return isEnabled() || ChromeFeatureList.sBackGestureActivityTabProvider.isEnabled();
    }

    /**
     * @return True if the tab navigation should be corrected on fallback callback.
     */
    public static boolean correctTabNavigationOnFallback() {
        return isEnabled() && TAB_HISTORY_RECOVER.getValue();
    }

    /**
     * @return True if app should be moved to back by manually calling `moveTaskToBack` when back is
     *     pressed during start up. Otherwise, call `onBackPressed` to trigger default behavior.
     */
    public static boolean shouldMoveToBackDuringStartup() {
        return ChromeFeatureList.sBackGestureMoveToBackDuringStartup.isEnabled();
    }

    /**
     * Record when the back press is consumed by a certain feature.
     * @param type The {@link Type} which consumes the back press event.
     */
    public static void record(@Type int type) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM, sMetricsMap.get(type), sMetricsMaxValue);
    }

    /**
     * Record when the back press is consumed by a custom tab.
     *
     * @param type The {@link Type} which consumes the back press event.
     * @param separateTask Whether the custom tab runs in a separate task.
     */
    public static void recordForCustomTab(@Type int type, boolean separateTask) {
        RecordHistogram.recordEnumeratedHistogram(
                separateTask ? HISTOGRAM_CUSTOM_TAB_SEPARATE_TASK : HISTOGRAM_CUSTOM_TAB_SAME_TASK,
                sMetricsMap.get(type),
                sMetricsMaxValue);
    }

    /**
     * @param type The {@link Type} of the back press handler.
     * @return The corresponding histogram value.
     */
    public static int getHistogramValue(@Type int type) {
        return sMetricsMap.get(type);
    }

    private static void recordFailure(@Type int type) {
        RecordHistogram.recordEnumeratedHistogram(
                FAILURE_HISTOGRAM, sMetricsMap.get(type), sMetricsMaxValue);
    }

    public BackPressManager() {
        mFallbackOnBackPressed = CallbackUtils.emptyRunnable();
        mUseSystemBack = MinimizeAppAndCloseTabBackPressHandler.shouldUseSystemBack();
        backPressStateChanged();
    }

    /**
     * Register the handler to intercept the back gesture.
     * @param handler Implementer of {@link BackPressHandler}.
     * @param type The {@link Type} of the handler.
     */
    public void addHandler(BackPressHandler handler, @Type int type) {
        assert mHandlers[type] == null : "Each type can have at most one handler";
        mHandlers[type] = handler;
        mObserverCallbacks[type] = (t) -> backPressStateChanged();
        handler.getHandleBackPressChangedSupplier().addObserver(mObserverCallbacks[type]);
        backPressStateChanged();
    }

    /**
     * Remove a registered handler. The methods of handler will not be called any more.
     * @param handler {@link BackPressHandler} to be removed.
     */
    public void removeHandler(@NonNull BackPressHandler handler) {
        for (int i = 0; i < mHandlers.length; i++) {
            if (mHandlers[i] == handler) {
                removeHandler(i);
                return;
            }
        }
    }

    /**
     * Remove a registered handler. The methods of handler will not be called any more.
     * @param type {@link Type} to be removed.
     */
    public void removeHandler(@Type int type) {
        BackPressHandler handler = mHandlers[type];
        handler.getHandleBackPressChangedSupplier().removeObserver(mObserverCallbacks[type]);
        mObserverCallbacks[type] = null;
        mHandlers[type] = null;
        backPressStateChanged();
    }

    /**
     * @param type The {@link Type} which needs to check.
     * @return True if a handler of this type has been registered.
     */
    public boolean has(@Type int type) {
        return mHandlers[type] != null;
    }

    /**
     * @return A {@link OnBackPressedCallback} which should be added to
     * {@link androidx.activity.OnBackPressedDispatcher}.
     */
    public OnBackPressedCallback getCallback() {
        return mCallback;
    }

    /*
     * @param fallbackOnBackPressed Callback executed when a handler claims to intercept back press
     *         but no handler succeeds.
     */
    public void setFallbackOnBackPressed(Runnable runnable) {
        mFallbackOnBackPressed = runnable;
    }

    /**
     * Set a callback fired when a back press is triggered. This is introduced to investigate data
     * inconsistency between experimental groups. and is not intended to be re-used.
     * TODO(crbug.com/40944523): remove after sufficient data is collected.
     */
    public void setOnBackPressedListener(Runnable callback) {
        mOnBackPressed = callback;
    }

    /**
     * Turn on more checks if a system back arm is available, such as when running on tabbed
     * activity.
     * @param hasSystemBackArm True if system back arm is feasible.
     */
    public void setHasSystemBackArm(boolean hasSystemBackArm) {
        mHasSystemBackArm = hasSystemBackArm;
    }

    /** Set a supplier to provide whether first visible content has been drawn. */
    public void setIsFirstVisibleContentDrawnSupplier(Supplier<Boolean> supplier) {
        mIsFirstVisibleContentDrawnSupplier = supplier;
    }

    /** Set a supplier to provide whether gesture nav mode is on when called. */
    public void setIsGestureNavEnabledSupplier(Supplier<Boolean> supplier) {
        mIsGestureNavEnabledSupplier = supplier;
    }

    /**
     * Record if back press occurs before first visible content is drawn. TODO(crbug.com/40944523):
     * remove after it is fixed.
     */
    public void recordSystemBackCountIfBeforeFirstVisibleContent() {
        if (mBackBeforeFirstVisibleContentRecorded) return;
        if (mIsFirstVisibleContentDrawnSupplier != null
                && mIsFirstVisibleContentDrawnSupplier.get()) return;
        mBackBeforeFirstVisibleContentRecorded = true;
        RecordUserAction.record("SystemBackBeforeFirstVisibleContent");
    }

    private void backPressStateChanged() {
        boolean intercept = shouldInterceptBackPress();
        if (mHasSystemBackArm) {
            // If not using system back and MINIMIZE_APP_AND_CLOSE_TAB has registered, this must be
            // true, since MINIMIZE_APP_AND_CLOSE_TAB unconditionally consumes last back press.
            if (has(Type.MINIMIZE_APP_AND_CLOSE_TAB) && !mUseSystemBack) {
                assert intercept : "Should always intercept back press on non-systemback group";
            }
            mCallback.setEnabled(intercept || !mUseSystemBack);
        } else {
            mCallback.setEnabled(intercept);
        }
    }

    @VisibleForTesting
    BackPressHandler getEnabledBackPressHandler() {
        for (int i = 0; i < mHandlers.length; i++) {
            BackPressHandler handler = mHandlers[i];
            if (handler == null) continue;
            Boolean enabled = handler.getHandleBackPressChangedSupplier().get();
            if (enabled != null && enabled) {
                return handler;
            }
        }
        assert false;
        return null;
    }

    private int getIndex(BackPressHandler handler) {
        for (int i = 0; i < mHandlers.length; i++) {
            if (mHandlers[i] == null) continue;
            if (mHandlers[i] == handler) {
                return i;
            }
        }
        throw new AssertionError("Handler not found.");
    }

    private void handleBackPress() {
        var failed = new ArrayList<String>();
        for (int i = 0; i < mHandlers.length; i++) {
            BackPressHandler handler = mHandlers[i];
            if (handler == null) continue;
            Boolean enabled = handler.getHandleBackPressChangedSupplier().get();
            if (enabled != null && enabled) {
                int res = handler.handleBackPress();
                mLastCalledHandlerType = i;
                if (res == BackPressResult.FAILURE) {
                    failed.add(i + "");
                    recordFailure(i);
                } else {
                    record(i);
                    assertListOfFailedHandlers(failed, i);
                    return;
                }
            }
        }
        if (mFallbackOnBackPressed != null) mFallbackOnBackPressed.run();
        assertListOfFailedHandlers(failed, -1);
        assert !failed.isEmpty() : "Callback is enabled but no handler consumed back gesture.";
    }

    @Override
    public void destroy() {
        for (int i = 0; i < mHandlers.length; i++) {
            if (has(i)) {
                removeHandler(i);
            }
        }
    }

    @VisibleForTesting
    boolean shouldInterceptBackPress() {
        for (BackPressHandler handler : mHandlers) {
            if (handler == null) continue;
            if (!Boolean.TRUE.equals(handler.getHandleBackPressChangedSupplier().get())) continue;
            return true;
        }
        return false;
    }

    private void assertListOfFailedHandlers(List<String> failed, int succeed) {
        if (failed.isEmpty()) return;
        var msg = String.join(", ", failed);
        assert false
                : String.format(
                        "%s didn't correctly handle back press; handled by %s.", msg, succeed);
    }

    public BackPressHandler[] getHandlersForTesting() {
        return mHandlers;
    }

    public int getLastCalledHandlerForTesting() {
        return mLastCalledHandlerType;
    }

    public void resetLastCalledHandlerForTesting() {
        mLastCalledHandlerType = -1;
    }

    public static String getHistogramForTesting() {
        return HISTOGRAM;
    }

    public static String getCustomTabSameTaskHistogramForTesting() {
        return HISTOGRAM_CUSTOM_TAB_SAME_TASK;
    }

    public static String getCustomTabSeparateTaskHistogramForTesting() {
        return HISTOGRAM_CUSTOM_TAB_SEPARATE_TASK;
    }
}
