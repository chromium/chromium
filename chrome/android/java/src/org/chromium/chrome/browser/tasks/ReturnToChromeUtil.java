// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Point;
import android.os.Bundle;
import android.text.TextUtils;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeInactivityTracker;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.components.cached_flags.IntCachedFieldTrialParameter;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/**
 * This is a utility class for managing features related to returning to Chrome after haven't used
 * Chrome for a while.
 */
public final class ReturnToChromeUtil {
    /**
     * The reasons of failing to show the home surface UI on a NTP.
     *
     * <p>These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused. See tools/metrics/histograms/enums.xml.
     */
    @IntDef({
        FailToShowHomeSurfaceReason.FAIL_TO_CREATE_NTP_TAB,
        FailToShowHomeSurfaceReason.FAIL_TO_FIND_NTP_TAB,
        FailToShowHomeSurfaceReason.NOT_A_NATIVE_PAGE,
        FailToShowHomeSurfaceReason.NOT_A_NTP_NATIVE_PAGE,
        FailToShowHomeSurfaceReason.NATIVE_PAGE_IS_FROZEN,
        FailToShowHomeSurfaceReason.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface FailToShowHomeSurfaceReason {
        int FAIL_TO_CREATE_NTP_TAB = 0;
        int FAIL_TO_FIND_NTP_TAB = 1;
        int NOT_A_NATIVE_PAGE = 2;

        int NOT_A_NTP_NATIVE_PAGE = 3;
        int NATIVE_PAGE_IS_FROZEN = 4;
        int NUM_ENTRIES = 5;
    }

    public static final String HOME_SURFACE_SHOWN_AT_STARTUP_UMA =
            "NewTabPage.AsHomeSurface.ShownAtStartup";
    public static final String HOME_SURFACE_SHOWN_UMA = "NewTabPage.AsHomeSurface";
    public static final String FAIL_TO_SHOW_HOME_SURFACE_UI_UMA =
            "NewTabPage.FailToShowHomeSurfaceUI";

    // Start return time experiment:
    // This parameter isn't just used on tablets anymore.
    public static final String HOME_SURFACE_RETURN_TIME_SECONDS_PARAM =
            "start_surface_return_time_on_tablet_seconds";
    public static final IntCachedFieldTrialParameter HOME_SURFACE_RETURN_TIME_SECONDS =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_RETURN_TIME,
                    HOME_SURFACE_RETURN_TIME_SECONDS_PARAM,
                    28800); // 8 hours

    private ReturnToChromeUtil() {}

    /**
     * Determine if we should show the tab switcher on returning to Chrome. Returns true if enough
     * time has elapsed since the app was last backgrounded or foreground, depending on which time
     * is the max. The threshold time in milliseconds is set by experiment
     * "enable-start-surface-return-time".
     *
     * @param lastTimeMillis The last time the application was backgrounded or foreground, depends
     *     on which time is the max. Set in ChromeTabbedActivity::onStopWithNative
     * @return true if past threshold, false if not past threshold or experiment cannot be loaded.
     */
    public static boolean shouldShowTabSwitcher(final long lastTimeMillis) {
        long tabSwitcherAfterMillis = getReturnTime(HOME_SURFACE_RETURN_TIME_SECONDS);

        if (lastTimeMillis == -1) {
            // No last background timestamp set, use control behavior unless "immediate" was set.
            return tabSwitcherAfterMillis == 0;
        }

        if (tabSwitcherAfterMillis < 0) {
            // If no value for experiment, use control behavior.
            return false;
        }

        return System.currentTimeMillis() - lastTimeMillis >= tabSwitcherAfterMillis;
    }

    /**
     * Gets the return time interval. The return time is in the unit of milliseconds.
     *
     * @param returnTime The return time parameter based on form factor, either phones or tablets.
     */
    private static long getReturnTime(IntCachedFieldTrialParameter returnTime) {
        return returnTime.getValue() * DateUtils.SECOND_IN_MILLIS;
    }

    /** Returns whether should show a NTP as the home surface at startup. */
    public static boolean shouldShowNtpAsHomeSurfaceAtStartup(
            Intent intent, Bundle bundle, ChromeInactivityTracker inactivityTracker) {
        // If the current session is due to recreated, don't show a NTP homepage.
        if (isFromRecreate(bundle)) {
            return false;
        }

        // Checks whether to show the NTP homepage due to feature flag
        // HOME_SURFACE_RETURN_TIME_SECONDS.
        long lastVisibleTimeMs = inactivityTracker.getLastVisibleTimeMs();
        long lastBackgroundTimeMs = inactivityTracker.getLastBackgroundedTimeMs();
        return IntentUtils.isMainIntentFromLauncher(intent)
                && ReturnToChromeUtil.shouldShowTabSwitcher(
                        Math.max(lastBackgroundTimeMs, lastVisibleTimeMs));
    }

    /** Returns whether a recreate was happened. */
    private static boolean isFromRecreate(Bundle bundle) {
        if (bundle == null) return false;

        return bundle.getBoolean(ChromeActivity.IS_FROM_RECREATING, false);
    }

    /**
     * Creates a new Tab and show Home surface UI. This is called when the last active Tab isn't a
     * NTP, and we need to create one and show Home surface UI (a module showing the last active
     * Tab).
     *
     * @param tabCreator The {@link TabCreator} object.
     * @param tabModelSelector The {@link TabModelSelector} object.
     * @param homeSurfaceTracker The {@link HomeSurfaceTracker} object.
     * @param lastActiveTabUrl The URL of the last active Tab. It is non-null in cold startup before
     *     the Tab is restored.
     * @param lastActiveTab The object of the last active Tab. It is non-null after TabModel is
     *     initialized, e.g., in warm startup.
     */
    public static Tab createNewTabAndShowHomeSurfaceUi(
            @NonNull TabCreator tabCreator,
            @NonNull HomeSurfaceTracker homeSurfaceTracker,
            @Nullable TabModelSelector tabModelSelector,
            @Nullable String lastActiveTabUrl,
            @Nullable Tab lastActiveTab) {
        assert lastActiveTab != null || lastActiveTabUrl != null;

        // Creates a new Tab if doesn't find an existing to reuse.
        Tab ntpTab =
                tabCreator.createNewTab(
                        new LoadUrlParams(UrlConstants.NTP_URL), TabLaunchType.FROM_STARTUP, null);
        boolean isNtpUrl = UrlUtilities.isNtpUrl(ntpTab.getUrl());
        assert isNtpUrl : "The URL of the newly created NTP doesn't match NTP URL!";
        if (!isNtpUrl) {
            recordFailToShowHomeSurfaceReasonUma(
                    FailToShowHomeSurfaceReason.FAIL_TO_CREATE_NTP_TAB);
            return null;
        }

        // In cold startup, we only have the URL of the last active Tab.
        if (lastActiveTab == null) {
            // If the last active Tab isn't ready yet, we will listen to the willAddTab() event and
            // find the Tab instance with the given last active Tab's URL. The last active Tab is
            // always the first one to be restored.
            assert lastActiveTabUrl != null;
            TabModelObserver observer =
                    new TabModelObserver() {
                        @Override
                        public void willAddTab(Tab tab, int type) {
                            boolean isTabExpected =
                                    TextUtils.equals(lastActiveTabUrl, tab.getUrl().getSpec());
                            assert isTabExpected
                                    : String.format(
                                            Locale.ENGLISH,
                                            "The URL of first Tab restored doesn't match the URL of"
                                                + " the last active Tab read from the Tab state"
                                                + " metadata file! Existing Tab count = %d. Last"
                                                + " active tab = %s. First tab = %s.",
                                            tabModelSelector.getModel(false).getCount(),
                                            lastActiveTabUrl,
                                            tab.getUrl().getSpec());
                            if (!isTabExpected) {
                                return;
                            }
                            showHomeSurfaceUiOnNtp(ntpTab, tab, homeSurfaceTracker);
                            tabModelSelector.getModel(false).removeObserver(this);
                        }

                        @Override
                        public void restoreCompleted() {
                            // This would be no-op if the observer has been removed in willAddTab().
                            tabModelSelector.getModel(false).removeObserver(this);
                        }
                    };
            tabModelSelector.getModel(false).addObserver(observer);
        } else {
            // In warm startup, the last active Tab is ready.
            showHomeSurfaceUiOnNtp(ntpTab, lastActiveTab, homeSurfaceTracker);
        }

        return ntpTab;
    }

    /**
     * Shows a NTP on warm startup on tablets if return time arrives. Only create a new NTP if there
     * isn't any existing NTP to reuse.
     *
     * @param isIncognito Whether the incognito mode is selected.
     * @param shouldShowNtpHomeSurfaceOnStartup Whether to show a NTP as home surface on startup.
     * @param currentTabModel The object of the current {@link TabModel}.
     * @param tabCreator The {@link TabCreator} object.
     * @param homeSurfaceTracker The {@link HomeSurfaceTracker} object.
     * @return whether an NTP was shown.
     */
    public static boolean setInitialOverviewStateOnResumeWithNtp(
            boolean isIncognito,
            boolean shouldShowNtpHomeSurfaceOnStartup,
            TabModel currentTabModel,
            TabCreator tabCreator,
            HomeSurfaceTracker homeSurfaceTracker) {
        if (isIncognito || !shouldShowNtpHomeSurfaceOnStartup) {
            return false;
        }

        int index = currentTabModel.index();
        Tab lastActiveTab = TabModelUtils.getCurrentTab(currentTabModel);
        // Early exits if there isn't any Tab, i.e., don't create a home surface.
        if (lastActiveTab == null) return false;

        // If the last active Tab is a NTP, we continue to show this NTP as it is now.
        if (UrlUtilities.isNtpUrl(lastActiveTab.getUrl())) {
            if (!homeSurfaceTracker.isHomeSurfaceTab(lastActiveTab)) {
                homeSurfaceTracker.updateHomeSurfaceAndTrackingTabs(lastActiveTab, null);
            }
        } else {
            int indexOfFirstNtp =
                    TabModelUtils.getTabIndexByUrl(currentTabModel, UrlConstants.NTP_URL);
            if (indexOfFirstNtp != TabModel.INVALID_TAB_INDEX) {
                Tab ntpTab = currentTabModel.getTabAt(indexOfFirstNtp);
                assert indexOfFirstNtp != index;
                boolean isNtpUrl = UrlUtilities.isNtpUrl(ntpTab.getUrl());
                assert isNtpUrl
                        : "The URL of the first NTP found onResume doesn't match a NTP URL!";
                if (!isNtpUrl) {
                    recordFailToShowHomeSurfaceReasonUma(
                            FailToShowHomeSurfaceReason.FAIL_TO_FIND_NTP_TAB);
                    return false;
                }

                // Sets the found NTP as home surface.
                TabModelUtils.setIndex(currentTabModel, indexOfFirstNtp);
                showHomeSurfaceUiOnNtp(ntpTab, lastActiveTab, homeSurfaceTracker);
            } else {
                // There isn't any existing NTP, create one.
                createNewTabAndShowHomeSurfaceUi(
                        tabCreator, homeSurfaceTracker, null, null, lastActiveTab);
            }
        }

        recordHomeSurfaceShownAtStartup();
        recordHomeSurfaceShown();
        return true;
    }

    /**
     * Records user clicks on the tab switcher button in New tab page.
     *
     * @param currentTab Current tab or null if none exists.
     */
    public static void recordClickTabSwitcher(@Nullable Tab currentTab) {
        if (currentTab != null
                && !currentTab.isIncognito()
                && UrlUtilities.isNtpUrl(currentTab.getUrl())) {
            BrowserUiUtils.recordModuleClickHistogram(ModuleTypeOnStartAndNtp.TAB_SWITCHER_BUTTON);
        }
    }

    /** Recorded when the home surface NTP is shown at startup. */
    public static void recordHomeSurfaceShownAtStartup() {
        RecordHistogram.recordBooleanHistogram(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true);
    }

    /** Records the home surface shown impressions. */
    public static void recordHomeSurfaceShown() {
        RecordHistogram.recordBooleanHistogram(HOME_SURFACE_SHOWN_UMA, true);
    }

    /**
     * Returns the start position of the context menu of a home module.
     *
     * @param resources The {@link Resources} instance to load Android resources from.
     */
    public static Point calculateContextMenuStartPosition(Resources resources) {
        // On the single tab module, the x starts from the right of the tab thumbnail.
        int contextMenuStartX =
                resources.getDimensionPixelSize(R.dimen.single_tab_module_lateral_margin)
                        + resources.getDimensionPixelSize(R.dimen.single_tab_module_padding_bottom)
                        + resources.getDimensionPixelSize(
                                R.dimen.single_tab_module_tab_thumbnail_size);
        // The y starts from the same height of the tab thumbnail.
        int contextMenuStartY =
                resources.getDimensionPixelSize(R.dimen.single_tab_module_padding_top) * 3;
        return new Point(contextMenuStartX, contextMenuStartY);
    }

    /** Shows the home surface UI on the given NTP on tablets. */
    static void showHomeSurfaceUiOnNtp(
            Tab ntpTab, Tab lastActiveTab, HomeSurfaceTracker homeSurfaceTracker) {
        NativePage nativePage = ntpTab.getNativePage();
        if (nativePage == null) {
            recordFailToShowHomeSurfaceReasonUma(FailToShowHomeSurfaceReason.NOT_A_NATIVE_PAGE);
            return;
        }

        // It is possible to get null after casting ntpTab.getNativePage() to NewTabPage, early
        // exit here. See https://crbug.com/1449900.
        if (!(nativePage instanceof NewTabPage)) {
            recordFailToShowHomeSurfaceReasonUma(FailToShowHomeSurfaceReason.NOT_A_NTP_NATIVE_PAGE);
            if (nativePage.isFrozen()) {
                recordFailToShowHomeSurfaceReasonUma(
                        FailToShowHomeSurfaceReason.NATIVE_PAGE_IS_FROZEN);
            }
            return;
        }

        // This cast is now guaranteed to succeed to a non-null value.
        NewTabPage newTabPage = (NewTabPage) nativePage;
        homeSurfaceTracker.updateHomeSurfaceAndTrackingTabs(ntpTab, lastActiveTab);
        if (HomeModulesMetricsUtils.useMagicStack()) {
            newTabPage.showMagicStack(lastActiveTab);
        } else {
            newTabPage.showHomeSurfaceUi(lastActiveTab);
        }
    }

    // TODO(crbug.com/40270227): Removes this histogram once we understand the root cause of
    // the crash.
    private static void recordFailToShowHomeSurfaceReasonUma(
            @FailToShowHomeSurfaceReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                FAIL_TO_SHOW_HOME_SURFACE_UI_UMA, reason, FailToShowHomeSurfaceReason.NUM_ENTRIES);
    }
}
