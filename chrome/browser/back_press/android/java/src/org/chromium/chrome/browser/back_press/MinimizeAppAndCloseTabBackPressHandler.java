// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.util.Pair;

import androidx.activity.BackEventCompat;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;

import java.util.Locale;
import java.util.Objects;
import java.util.function.Predicate;

/**
 * The back press handler as the final step of back press handling. This is always enabled in order
 * to manually minimize app and close tab if necessary.
 */
public class MinimizeAppAndCloseTabBackPressHandler implements BackPressHandler, Destroyable {
    private static final String TAG = "MinimizeAppCloseTab";
    private static final String HANDLE_BACK_STARTED = "handleOnBackStarted";
    private static final String HANDLE_BACK_PRESSED = "handleBackPress";
    static final String HISTOGRAM = "Android.BackPress.MinimizeAppAndCloseTab";
    static final String HISTOGRAM_CUSTOM_TAB_SAME_TASK =
            "Android.BackPress.MinimizeAppAndCloseTab.CustomTab.SameTask";
    static final String HISTOGRAM_CUSTOM_TAB_SEPARATE_TASK =
            "Android.BackPress.MinimizeAppAndCloseTab.CustomTab.SeparateTask";

    // A conditionally enabled supplier, whose value is false only when minimizing app without
    // closing tabs in the 'system back' arm.
    private final ObservableSupplierImpl<Boolean> mSystemBackPressSupplier =
            new ObservableSupplierImpl<>();

    // An always-enabled supplier since this handler is the final step of back press handling.
    private final ObservableSupplierImpl<Boolean> mNonSystemBackPressSupplier =
            new ObservableSupplierImpl<>();
    private final Predicate<Tab> mBackShouldCloseTab;
    private final Callback<Tab> mSendToBackground;
    private final Callback<Tab> mOnTabChanged = this::onTabChanged;
    private final ObservableSupplier<Tab> mActivityTabSupplier;
    private final Runnable mCallbackOnBackPress;
    private final Supplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private int mLayoutTypeOnStart;
    private Tab mObservedTab;
    private Tab mTabOnStart;
    private final boolean mUseSystemBack;

    private static Integer sVersionForTesting;

    @IntDef({
        MinimizeAppAndCloseTabType.MINIMIZE_APP,
        MinimizeAppAndCloseTabType.CLOSE_TAB,
        MinimizeAppAndCloseTabType.MINIMIZE_APP_AND_CLOSE_TAB,
        MinimizeAppAndCloseTabType.NUM_TYPES
    })
    public @interface MinimizeAppAndCloseTabType {
        int MINIMIZE_APP = 0;
        int CLOSE_TAB = 1;
        int MINIMIZE_APP_AND_CLOSE_TAB = 2;
        int NUM_TYPES = 3;
    }

    /**
     * Record metrics of how back press is finally consumed by the app.
     * @param type The action we do when back press is consumed.
     */
    public static void record(@MinimizeAppAndCloseTabType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM, type, MinimizeAppAndCloseTabType.NUM_TYPES);
    }

    /**
     * Record metrics of how back press is finally consumed by the custom tab.
     *
     * @param type The action we do when back press is consumed.
     * @param separateTask Whether the custom tab runs in a separate task.
     */
    public static void recordForCustomTab(
            @MinimizeAppAndCloseTabType int type, boolean separateTask) {
        RecordHistogram.recordEnumeratedHistogram(
                separateTask ? HISTOGRAM_CUSTOM_TAB_SEPARATE_TASK : HISTOGRAM_CUSTOM_TAB_SAME_TASK,
                type,
                MinimizeAppAndCloseTabType.NUM_TYPES);
    }

    public static void assertOnLastBackPress(
            @Nullable Tab currentTab,
            @Nullable Tab activityTab,
            Predicate<Tab> backShouldCloseTab,
            Supplier<LayoutStateProvider> layoutStateProviderSupplier,
            boolean isActivityFinishingOrDestroyed) {
        boolean expectedShouldClose = false;
        boolean expectedShouldMinimize = currentTab == null;
        if (currentTab != null) {
            expectedShouldClose = backShouldCloseTab.test(currentTab);
            expectedShouldMinimize =
                    !expectedShouldClose || TabAssociatedApp.isOpenedFromExternalApp(currentTab);
        }

        boolean actualShouldClose = false;
        boolean actualShouldMinimize = activityTab == null;
        if (activityTab != null) {
            actualShouldClose = backShouldCloseTab.test(activityTab);
            actualShouldMinimize =
                    !actualShouldClose || TabAssociatedApp.isOpenedFromExternalApp(activityTab);
        }

        var layoutStateProvider = layoutStateProviderSupplier.get();
        int layoutType =
                layoutStateProvider == null
                        ? LayoutType.NONE
                        : layoutStateProvider.getActiveLayoutType();

        String msg =
                "Unexpected minimizeApp state: expect %s %s %s; actual %s %s %s; "
                        + "layoutType %s; destroy %s";
        assert (actualShouldClose == expectedShouldClose)
                        && (actualShouldMinimize == expectedShouldMinimize)
                : String.format(
                        msg,
                        currentTab,
                        expectedShouldClose,
                        expectedShouldMinimize,
                        activityTab,
                        actualShouldClose,
                        actualShouldMinimize,
                        layoutType,
                        isActivityFinishingOrDestroyed);
    }

    /**
     * @param activityTabSupplier Supplier giving the current interact-able tab.
     * @param backShouldCloseTab Test whether the current tab should be closed on back press.
     * @param sendToBackground Callback when app should be sent to background on back press.
     * @param callbackOnBackPress Callback when back press is handled.
     */
    public MinimizeAppAndCloseTabBackPressHandler(
            ObservableSupplier<Tab> activityTabSupplier,
            Predicate<Tab> backShouldCloseTab,
            Callback<Tab> sendToBackground,
            Runnable callbackOnBackPress,
            Supplier<LayoutStateProvider> layoutStateProviderSupplier) {
        mBackShouldCloseTab = backShouldCloseTab;
        mSendToBackground = sendToBackground;
        mActivityTabSupplier = activityTabSupplier;
        mUseSystemBack = shouldUseSystemBack();
        mNonSystemBackPressSupplier.set(true);
        mCallbackOnBackPress = callbackOnBackPress;
        mLayoutStateProviderSupplier = layoutStateProviderSupplier;

        mActivityTabSupplier.addObserver(mOnTabChanged);
        // Init system back arm, using the current tab to determine whether back press should be
        // handled.
        onTabChanged(mActivityTabSupplier.get());
    }

    @Override
    public void handleOnBackStarted(BackEventCompat backEvent) {
        mLayoutTypeOnStart = getLayoutType();
        mTabOnStart = mActivityTabSupplier.get();

        Pair<Boolean, Boolean> backPressAction = determineBackPressAction(mTabOnStart);
        assertBackPressState(
                /* minimizeApp= */ backPressAction.first,
                /* shouldCloseTab= */ backPressAction.second,
                mActivityTabSupplier.get(),
                HANDLE_BACK_STARTED);
    }

    @Override
    public @BackPressResult int handleBackPress() {
        Tab currentTab = mActivityTabSupplier.get();
        Pair<Boolean, Boolean> backPressAction = determineBackPressAction(currentTab);
        boolean minimizeApp = backPressAction.first;
        boolean shouldCloseTab = backPressAction.second;

        if (currentTab != null) {
            // TAB history handler has a higher priority and should navigate page back before
            // minimizing app and closing tab.
            if (currentTab.canGoBack()) {
                assert false : "Tab should be navigated back before closing or exiting app";
                if (BackPressManager.correctTabNavigationOnFallback()) {
                    return BackPressResult.FAILURE;
                }
            }
            // At this point we know either the tab will close or the app will minimize.
            NativePage nativePage = currentTab.getNativePage();
            if (nativePage != null) {
                nativePage.notifyHidingWithBack();
            }
        }

        mCallbackOnBackPress.run();

        assertBackPressState(minimizeApp, shouldCloseTab, currentTab, HANDLE_BACK_PRESSED);

        if (minimizeApp) {
            record(
                    shouldCloseTab
                            ? MinimizeAppAndCloseTabType.MINIMIZE_APP_AND_CLOSE_TAB
                            : MinimizeAppAndCloseTabType.MINIMIZE_APP);
            // If system back is enabled, we should let system handle the back press when
            // no tab is about to be closed.
            assert shouldCloseTab || !mUseSystemBack;
            mSendToBackground.onResult(shouldCloseTab ? currentTab : null);
        } else { // shouldCloseTab is always true if minimizeApp is false.
            record(MinimizeAppAndCloseTabType.CLOSE_TAB);
            WebContents webContents = currentTab.getWebContents();
            if (webContents != null) webContents.dispatchBeforeUnload(false);
        }

        return BackPressResult.SUCCESS;
    }

    private Pair<Boolean, Boolean> determineBackPressAction(Tab currentTab) {
        boolean minimizeApp;
        boolean shouldCloseTab;

        if (currentTab == null) {
            assert !mUseSystemBack
                    : "Should be disabled when there is no valid tab and back press is consumed.";
            minimizeApp = true;
            shouldCloseTab = false;
        } else {
            shouldCloseTab = mBackShouldCloseTab.test(currentTab);

            // Minimize the app if either:
            // - we decided not to close the tab
            // - we decided to close the tab, but it was opened by an external app, so we will go
            //   exit Chrome on top of closing the tab
            minimizeApp = !shouldCloseTab || TabAssociatedApp.isOpenedFromExternalApp(currentTab);
        }

        return new Pair(minimizeApp, shouldCloseTab);
    }

    private void assertBackPressState(
            boolean minimizeApp, boolean shouldCloseTab, Tab currentTab, String caller) {
        final boolean minimizeAppWithoutClosingTab = minimizeApp && !shouldCloseTab;

        // The two experiment arms behave differently only when minimizing app without closing tab.
        if (!Objects.equals(mSystemBackPressSupplier.get(), mNonSystemBackPressSupplier.get())
                && !minimizeAppWithoutClosingTab) {
            String msg =
                    String.format(
                            Locale.US,
                            "%s - system back arm %s; should close %s; minimize %s; current tab %s,"
                                    + " observed tab %s; tab on start %s; open from external %s; "
                                    + "system back arm supplier %s, non back supplier %s; layout on"
                                    + " start: %s, on pressed: %s nav mode %s;",
                            caller,
                            mUseSystemBack,
                            shouldCloseTab,
                            minimizeApp,
                            currentTab,
                            mObservedTab,
                            mTabOnStart,
                            currentTab != null
                                    && TabAssociatedApp.isOpenedFromExternalApp(currentTab),
                            mSystemBackPressSupplier.get(),
                            mNonSystemBackPressSupplier.get(),
                            mLayoutTypeOnStart,
                            getLayoutType(),
                            getNavigationMode());
            assert false : msg;
            Log.i(TAG, msg);
        }

        if (minimizeAppWithoutClosingTab && mSystemBackPressSupplier.get()) {
            String msg =
                    String.format(
                            Locale.US,
                            "%s - system back arm should not consume back event for minimizing app"
                                    + " only. system back arm %s; currentTab %s; mObservedTab %s;"
                                    + " mTabOnStart %s; open from external %s; layout on start: %s,"
                                    + " on pressed: %s; nav mode %s",
                            caller,
                            mUseSystemBack,
                            currentTab,
                            mObservedTab,
                            mTabOnStart,
                            currentTab != null
                                    && TabAssociatedApp.isOpenedFromExternalApp(currentTab),
                            mLayoutTypeOnStart,
                            getLayoutType(),
                            getNavigationMode());
            assert false : msg;
            Log.i(TAG, msg);
        }

        // mLayoutTypeOnStart should not be NONE if handleOnBackStarted is called.
        if (mTabOnStart != currentTab
                && (VERSION.SDK_INT > VERSION_CODES.TIRAMISU
                        || mLayoutTypeOnStart != LayoutType.NONE)) {
            final var actionOnStart = determineBackPressAction(mTabOnStart);
            if (actionOnStart.first != minimizeApp || actionOnStart.second != shouldCloseTab) {
                var msg =
                        String.format(
                                Locale.US,
                                "%s - tab is changed during gesture. system back arm %s;"
                                        + " currentTab %s minimize %s close %s;  mTabOnStart %s"
                                        + " minimize %s close %s; open from external %s; layout on"
                                        + " start: %s, on pressed: %s; nav mode %s",
                                caller,
                                mUseSystemBack,
                                currentTab,
                                minimizeApp,
                                shouldCloseTab,
                                mTabOnStart,
                                actionOnStart.first,
                                actionOnStart.second,
                                currentTab != null
                                        && TabAssociatedApp.isOpenedFromExternalApp(currentTab),
                                mLayoutTypeOnStart,
                                getLayoutType(),
                                getNavigationMode());
                assert false : msg;
                Log.i(TAG, msg);
            }
        }
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mUseSystemBack ? mSystemBackPressSupplier : mNonSystemBackPressSupplier;
    }

    @Override
    public void destroy() {
        mActivityTabSupplier.removeObserver(mOnTabChanged);
    }

    private void onTabChanged(Tab tab) {
        mObservedTab = tab;
        mSystemBackPressSupplier.set(tab != null && mBackShouldCloseTab.test(tab));
    }

    private int getLayoutType() {
        return mLayoutStateProviderSupplier.hasValue()
                ? mLayoutStateProviderSupplier.get().getActiveLayoutType()
                : LayoutType.NONE;
    }

    private String getNavigationMode() {
        final Tab tab = mActivityTabSupplier.get();
        if (tab == null
                || tab.getWindowAndroid() == null
                || tab.getWindowAndroid().getWindow() == null) return "unknown";
        return UiUtils.isGestureNavigationMode(tab.getWindowAndroid().getWindow())
                ? "gesture"
                : "3-button";
    }

    static boolean shouldUseSystemBack() {
        // https://developer.android.com/about/versions/12/behavior-changes-all#back-press
        // Starting from 12, root launcher activities are no longer finished on Back press.
        // Limiting to T, since some OEMs seem to still finish activity on 12.
        boolean isAtLeastT =
                (sVersionForTesting == null ? VERSION.SDK_INT : sVersionForTesting)
                        >= VERSION_CODES.TIRAMISU;
        return isAtLeastT && ChromeFeatureList.sBackToHomeAnimation.isEnabled();
    }

    static void setVersionForTesting(Integer version) {
        sVersionForTesting = version;
        ResettersForTesting.register(() -> sVersionForTesting = null);
    }

    public static String getHistogramNameForTesting() {
        return HISTOGRAM;
    }

    public static String getCustomTabSameTaskHistogramNameForTesting() {
        return HISTOGRAM_CUSTOM_TAB_SAME_TASK;
    }

    public static String getCustomTabSeparateTaskHistogramNameForTesting() {
        return HISTOGRAM_CUSTOM_TAB_SEPARATE_TASK;
    }
}
