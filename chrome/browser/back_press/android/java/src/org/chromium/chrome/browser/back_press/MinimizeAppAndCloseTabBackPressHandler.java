// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.util.Pair;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.OnSystemNavigationObserver;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.Predicate;

/**
 * The back press handler as the final step of back press handling. This is always enabled in order
 * to manually minimize app and close tab if necessary.
 */
@NullMarked
public class MinimizeAppAndCloseTabBackPressHandler
        implements BackPressHandler, OnSystemNavigationObserver, Destroyable {
    static final String HISTOGRAM = "Android.BackPress.MinimizeAppAndCloseTab";
    static final String HISTOGRAM_TAB_CLOSURE = "Android.BackPress.TabClosureType";
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
    private final Predicate<Tab> mMinimizationShouldCloseTab;
    private final Callback<@Nullable Tab> mSendToBackground;
    private final Callback<Tab> mCloseTabUponMinimization;
    private final Callback<Tab> mOnTabChanged = this::onTabChanged;
    private final ObservableSupplier<Tab> mActivityTabSupplier;
    private final boolean mUseSystemBack;

    private static @Nullable Integer sVersionForTesting;

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

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange(TabClosureType)
    @IntDef({
        TabClosureType.WITHOUT_MINIMIZATION,
        TabClosureType.CHROME_MINIMIZATION,
        TabClosureType.OS_MINIMIZATION,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabClosureType {
        int WITHOUT_MINIMIZATION = 0;
        int CHROME_MINIMIZATION = 1;
        int OS_MINIMIZATION = 2;
        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:BackPressTabCloseType)

    /** Whether the feature of closing tab during minimization is supported. */
    public static boolean supportCloseTabUponMinimization() {
        boolean isAtLeastB =
                (sVersionForTesting == null ? VERSION.SDK_INT : sVersionForTesting)
                        >= VERSION_CODES.BAKLAVA;
        return isAtLeastB;
    }

    /**
     * Record metrics of how back press is finally consumed by the app.
     *
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

    /**
     * This method records when the tab is closed by the back press.
     *
     * @param type The type of the scenario in which the tab is closed.
     */
    private static void recordTabClosureType(@TabClosureType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_TAB_CLOSURE, type, TabClosureType.NUM_ENTRIES);
    }

    /**
     * @param activityTabSupplier Supplier giving the current interact-able tab.
     * @param backShouldCloseTab Test whether the back press should be intercepted to close tab.
     * @param minimizationShouldCloseTab Test whether the tab should be closed during minimization.
     * @param closeTabUponMinimization Callback triggered during minimization to close tab.
     * @param sendToBackground Callback when app should be sent to background on back press.
     */
    public MinimizeAppAndCloseTabBackPressHandler(
            ObservableSupplier<Tab> activityTabSupplier,
            Predicate<Tab> backShouldCloseTab,
            Predicate<Tab> minimizationShouldCloseTab,
            Callback<Tab> closeTabUponMinimization,
            Callback<@Nullable Tab> sendToBackground) {
        mBackShouldCloseTab = backShouldCloseTab;
        mMinimizationShouldCloseTab = minimizationShouldCloseTab;
        mCloseTabUponMinimization = closeTabUponMinimization;
        mSendToBackground = sendToBackground;
        mActivityTabSupplier = activityTabSupplier;
        mUseSystemBack = shouldUseSystemBack();
        mNonSystemBackPressSupplier.set(true);

        mActivityTabSupplier.addObserver(mOnTabChanged);
        // Init system back arm, using the current tab to determine whether back press should be
        // handled.
        onTabChanged(mActivityTabSupplier.get());
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
            }
            // At this point we know either the tab will close or the app will minimize.
            NativePage nativePage = currentTab.getNativePage();
            if (nativePage != null) {
                nativePage.notifyHidingWithBack();
            }
        }

        if (supportCloseTabUponMinimization()) assert !minimizeApp : "Should be minimized by OS";

        if (minimizeApp) {
            record(
                    shouldCloseTab
                            ? MinimizeAppAndCloseTabType.MINIMIZE_APP_AND_CLOSE_TAB
                            : MinimizeAppAndCloseTabType.MINIMIZE_APP);
            if (shouldCloseTab) recordTabClosureType(TabClosureType.CHROME_MINIMIZATION);
            // If system back is enabled, we should let system handle the back press when
            // no tab is about to be closed.
            assert shouldCloseTab || !mUseSystemBack;
            mSendToBackground.onResult(shouldCloseTab ? currentTab : null);
        } else { // shouldCloseTab is always true if minimizeApp is false.
            record(MinimizeAppAndCloseTabType.CLOSE_TAB);
            recordTabClosureType(TabClosureType.WITHOUT_MINIMIZATION);
            WebContents webContents = currentTab.getWebContents();
            if (webContents != null) webContents.dispatchBeforeUnload(false);
        }

        return BackPressResult.SUCCESS;
    }

    @Override
    public void onSystemNavigation() {
        Tab currentTab = mActivityTabSupplier.get();
        if (currentTab != null && mMinimizationShouldCloseTab.test(currentTab)) {
            mCloseTabUponMinimization.onResult(currentTab);
            recordTabClosureType(TabClosureType.OS_MINIMIZATION);
        }
    }

    @Override
    public boolean invokeBackActionOnEscape() {
        return false;
    }

    private Pair<Boolean, Boolean> determineBackPressAction(@Nullable Tab currentTab) {
        boolean minimizeApp;
        boolean shouldCloseTab;

        if (currentTab == null) {
            minimizeApp = true;
            shouldCloseTab = false;
        } else {
            shouldCloseTab = mBackShouldCloseTab.test(currentTab);

            // Minimize the app if either:
            // - we decided not to close the tab
            // - we decided to close the tab, but this can be closed during minimization.
            minimizeApp = !shouldCloseTab || mMinimizationShouldCloseTab.test(currentTab);
        }

        return new Pair(minimizeApp, shouldCloseTab);
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
        if (supportCloseTabUponMinimization()) {
            Pair<Boolean, Boolean> backPressAction = determineBackPressAction(tab);
            boolean minimizeApp = backPressAction.first;
            mSystemBackPressSupplier.set(!minimizeApp);
        } else {
            mSystemBackPressSupplier.set(tab != null && mBackShouldCloseTab.test(tab));
        }
    }

    static boolean shouldUseSystemBack() {
        // https://developer.android.com/about/versions/12/behavior-changes-all#back-press
        // Starting from 12, root launcher activities are no longer finished on Back press.
        // Limiting to T, since some OEMs seem to still finish activity on 12.
        boolean isAtLeastT =
                (sVersionForTesting == null ? VERSION.SDK_INT : sVersionForTesting)
                        >= VERSION_CODES.TIRAMISU;
        return isAtLeastT;
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
