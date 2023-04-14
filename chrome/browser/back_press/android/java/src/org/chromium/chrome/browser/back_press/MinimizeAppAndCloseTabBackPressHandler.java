// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
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

import java.util.function.Predicate;

/**
 * The back press handler as the final step of back press handling. This is always enabled in order
 * to manually minimize app and close tab if necessary.
 */
public class MinimizeAppAndCloseTabBackPressHandler implements BackPressHandler, Destroyable {
    static final String HISTOGRAM = "Android.BackPress.MinimizeAppAndCloseTab";

    // An always-enabled supplier since this handler is the final step of back press handling.
    private final ObservableSupplierImpl<Boolean> mBackPressSupplier =
            new ObservableSupplierImpl<>();
    private final Predicate<Tab> mBackShouldCloseTab;
    private final Callback<Tab> mSendToBackground;
    private final Callback<Tab> mOnTabChanged = this::onTabChanged;
    private final ObservableSupplier<Tab> mActivityTabSupplier;
    private final Runnable mCallbackOnBackPress;
    private final boolean mUseSystemBack;

    private static Integer sVersionForTesting;

    @IntDef({MinimizeAppAndCloseTabType.MINIMIZE_APP, MinimizeAppAndCloseTabType.CLOSE_TAB,
            MinimizeAppAndCloseTabType.MINIMIZE_APP_AND_CLOSE_TAB,
            MinimizeAppAndCloseTabType.NUM_TYPES})
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

    public static void assertOnLastBackPress(@Nullable Tab currentTab, @Nullable Tab activityTab,
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
        int layoutType = layoutStateProvider == null ? LayoutType.NONE
                                                     : layoutStateProvider.getActiveLayoutType();

        String msg = "Unexpected minimizeApp state: expect %s %s %s; actual %s %s %s; "
                + "layoutType %s; destroy %s";
        assert (actualShouldClose == expectedShouldClose)
                && (actualShouldMinimize == expectedShouldMinimize)
            : String.format(msg, currentTab, expectedShouldClose, expectedShouldMinimize,
                    activityTab, actualShouldClose, actualShouldMinimize, layoutType,
                    isActivityFinishingOrDestroyed);
    }

    /**
     * @param activityTabSupplier Supplier giving the current interact-able tab.
     * @param backShouldCloseTab Test whether the current tab should be closed on back press.
     * @param sendToBackground Callback when app should be sent to background on back press.
     * @param callbackOnBackPress Callback when back press is handled.
     */
    public MinimizeAppAndCloseTabBackPressHandler(ObservableSupplier<Tab> activityTabSupplier,
            Predicate<Tab> backShouldCloseTab, Callback<Tab> sendToBackground,
            Runnable callbackOnBackPress) {
        mBackShouldCloseTab = backShouldCloseTab;
        mSendToBackground = sendToBackground;
        mActivityTabSupplier = activityTabSupplier;
        mUseSystemBack = shouldUseSystemBack();
        mBackPressSupplier.set(!mUseSystemBack);
        mCallbackOnBackPress = callbackOnBackPress;
        if (mUseSystemBack) {
            mActivityTabSupplier.addObserver(mOnTabChanged);
        }
    }

    @Override
    public @BackPressResult int handleBackPress() {
        boolean minimizeApp;
        boolean shouldCloseTab;
        Tab currentTab = mActivityTabSupplier.get();

        if (currentTab == null) {
            assert !mUseSystemBack
                : "Should be disabled when there is no valid tab and back press will be consumed.";
            minimizeApp = true;
            shouldCloseTab = false;
        } else {
            // At this point we know either the tab will close or the app will minimize.
            NativePage nativePage = currentTab.getNativePage();
            if (nativePage != null) {
                nativePage.notifyHidingWithBack();
            }

            shouldCloseTab = mBackShouldCloseTab.test(currentTab);

            // Minimize the app if either:
            // - we decided not to close the tab
            // - we decided to close the tab, but it was opened by an external app, so we will go
            //   exit Chrome on top of closing the tab
            minimizeApp = !shouldCloseTab || TabAssociatedApp.isOpenedFromExternalApp(currentTab);
        }

        mCallbackOnBackPress.run();

        if (minimizeApp) {
            record(shouldCloseTab ? MinimizeAppAndCloseTabType.MINIMIZE_APP_AND_CLOSE_TAB
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

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressSupplier;
    }

    @Override
    public void destroy() {
        mActivityTabSupplier.removeObserver(mOnTabChanged);
    }

    private void onTabChanged(Tab tab) {
        assert mUseSystemBack : "Should not observe changes when system back is disabled";
        mBackPressSupplier.set(tab != null && mBackShouldCloseTab.test(tab));
    }

    private static boolean shouldUseSystemBack() {
        // https://developer.android.com/about/versions/12/behavior-changes-all#back-press
        // Starting from 12, root launcher activities are no longer finished on Back press.
        // Limiting to T, since some OEMs seem to still finish activity on 12.
        boolean isAtLeastT = (sVersionForTesting == null ? VERSION.SDK_INT : sVersionForTesting)
                >= VERSION_CODES.TIRAMISU;
        return isAtLeastT
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.BACK_GESTURE_REFACTOR, "system_back", false);
    }

    static void setVersionForTesting(Integer version) {
        sVersionForTesting = version;
    }

    public static String getHistogramNameForTesting() {
        return HISTOGRAM;
    }
}
