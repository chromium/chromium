// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Instrumentation;
import android.view.View;
import android.widget.Spinner;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridgeJni;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragmentAdvanced;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.browsing_data.TimePeriodUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Tests for quick delete controller. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.QUICK_DELETE_FOR_ANDROID)
@Batch(Batch.PER_CLASS)
public class QuickDeleteControllerTest {
    private static final long ACTIVITY_WAIT_LONG_MS = TimeUnit.SECONDS.toMillis(10);
    private static final long FIFTEEN_MINUTES_IN_MS = TimeUnit.MINUTES.toMillis(15);

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private BrowsingDataBridge.Natives mBrowsingDataBridgeMock;

    private final CallbackHelper mCallbackHelper = new CallbackHelper();

    private ChromeTabbedActivity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();

        mJniMocker.mock(BrowsingDataBridgeJni.TEST_HOOKS, mBrowsingDataBridgeMock);
        // Ensure that whenever the mock is asked to clear browsing data, the callback is
        // immediately called.
        doAnswer(
                        (Answer<Void>)
                                invocation -> {
                                    ((BrowsingDataBridge.OnClearBrowsingDataListener)
                                                    invocation.getArgument(2))
                                            .onBrowsingDataCleared();
                                    mCallbackHelper.notifyCalled();
                                    return null;
                                })
                .when(mBrowsingDataBridgeMock)
                .clearBrowsingData(
                        any(), any(), any(), any(), anyInt(), any(), any(), any(), any());

        // Set the time for the initial tab to be outside of the quick delete time span.
        Tab initialTab = mActivity.getActivityTab();
        runOnUiThreadBlocking(
                () -> {
                    TabTestUtils.setLastNavigationCommittedTimestampMillis(
                            initialTab, System.currentTimeMillis() - FIFTEEN_MINUTES_IN_MS);
                });

        // Open new tab for tests.
        mActivityTestRule.loadUrlInNewTab("about:blank");
    }

    @After
    public void tearDown() {
        // Close all tabs
        runOnUiThreadBlocking(
                () ->
                        mActivity
                                .getCurrentTabModel()
                                .closeTabs(TabClosureParams.closeAllTabs().build()));
    }

    private void openQuickDeleteDialog() {
        // Open 3 dot menu.
        runOnUiThreadBlocking(
                () -> {
                    AppMenuTestSupport.showAppMenu(
                            mActivityTestRule.getAppMenuCoordinator(), null, false);
                });
        onViewWaiting(withId(R.id.app_menu_list))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        // Click on quick delete menu item.
        runOnUiThreadBlocking(
                () -> {
                    AppMenuTestSupport.callOnItemClick(
                            mActivityTestRule.getAppMenuCoordinator(), R.id.quick_delete_menu_id);
                });
        onViewWaiting(withId(R.id.quick_delete_spinner), true)
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
    }

    private void assertDataTypesCleared(@TimePeriod int timePeriod, int... types) {
        verify(mBrowsingDataBridgeMock)
                .clearBrowsingData(
                        any(), any(), any(), eq(types), eq(timePeriod), any(), any(), any(), any());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "b/322945246")
    public void testNavigateToTabSwitcher_WhenClickingDelete() throws TimeoutException {
        openQuickDeleteDialog();
        onViewWaiting(withId(R.id.positive_button)).perform(click());

        // Wait for browsing data deletion to complete.
        mCallbackHelper.waitForOnly();

        LayoutTestUtils.waitForLayout(mActivity.getLayoutManager(), LayoutType.TAB_SWITCHER);
    }

    @Test
    @MediumTest
    public void testSnackbarShown_WhenClickingDelete() throws TimeoutException {
        openQuickDeleteDialog();
        onViewWaiting(withId(R.id.positive_button)).perform(click());

        // Wait for browsing data deletion to complete.
        mCallbackHelper.waitForOnly();

        onViewWaiting(
                        withText(
                                mActivity.getString(
                                        R.string.quick_delete_snackbar_message,
                                        TimePeriodUtils.getTimePeriodString(
                                                mActivity, TimePeriod.LAST_15_MINUTES))))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSnackbarShown_WhenClickingDelete_AllTimeSelected() throws TimeoutException {
        openQuickDeleteDialog();

        onView(withId(R.id.quick_delete_spinner)).check(matches(isDisplayed()));
        View dialogView =
                mActivity
                        .getModalDialogManager()
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CUSTOM_VIEW);

        runOnUiThreadBlocking(
                () -> {
                    Spinner spinnerView = dialogView.findViewById(R.id.quick_delete_spinner);
                    spinnerView.setSelection(5);
                    var option =
                            (TimePeriodUtils.TimePeriodSpinnerOption) spinnerView.getSelectedItem();
                    assertEquals(TimePeriod.ALL_TIME, option.getTimePeriod());
                });

        onViewWaiting(withId(R.id.positive_button)).perform(click());

        // Wait for browsing data deletion to complete.
        mCallbackHelper.waitForOnly();

        onViewWaiting(withText(R.string.quick_delete_snackbar_all_time_message))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testQuickDeleteHistogram_WhenClickingDelete() throws TimeoutException {
        openQuickDeleteDialog();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Privacy.QuickDelete.TabsEnabled", true)
                        .expectIntRecord(
                                QuickDeleteMetricsDelegate.HISTOGRAM_NAME,
                                QuickDeleteMetricsDelegate.QuickDeleteAction.DELETE_CLICKED)
                        .build();

        onViewWaiting(withId(R.id.positive_button)).perform(click());

        // Wait for browsing data deletion to complete.
        mCallbackHelper.waitForOnly();

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testDeleteBrowsingDataHistogram_WhenClickingDelete() throws TimeoutException {
        openQuickDeleteDialog();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Privacy.DeleteBrowsingData.Action", DeleteBrowsingDataAction.QUICK_DELETE);

        onViewWaiting(withId(R.id.positive_button)).perform(click());

        // Wait for browsing data deletion to complete.
        mCallbackHelper.waitForOnly();

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testBrowsingDataDeletion_DefaultTimePeriodSelected() throws TimeoutException {
        openQuickDeleteDialog();

        View dialogView =
                mActivity
                        .getModalDialogManager()
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CUSTOM_VIEW);

        runOnUiThreadBlocking(
                () -> {
                    Spinner spinnerView = dialogView.findViewById(R.id.quick_delete_spinner);
                    var option =
                            (TimePeriodUtils.TimePeriodSpinnerOption) spinnerView.getSelectedItem();
                    assertEquals(TimePeriod.LAST_15_MINUTES, option.getTimePeriod());
                });

        onViewWaiting(withId(R.id.positive_button)).perform(click());

        // Wait for browsing data deletion to complete.
        mCallbackHelper.waitForOnly();

        assertDataTypesCleared(
                TimePeriod.LAST_15_MINUTES,
                BrowsingDataType.HISTORY,
                BrowsingDataType.SITE_DATA,
                BrowsingDataType.CACHE);
    }

    @Test
    @MediumTest
    public void testBrowsingDataDeletion_LastHourSelected() throws TimeoutException {
        openQuickDeleteDialog();

        View dialogView =
                mActivity
                        .getModalDialogManager()
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CUSTOM_VIEW);

        runOnUiThreadBlocking(
                () -> {
                    Spinner spinnerView = dialogView.findViewById(R.id.quick_delete_spinner);
                    // Set the time selection for LAST_HOUR.
                    spinnerView.setSelection(1);
                    var option =
                            (TimePeriodUtils.TimePeriodSpinnerOption) spinnerView.getSelectedItem();
                    assertEquals(TimePeriod.LAST_HOUR, option.getTimePeriod());
                });

        onViewWaiting(withId(R.id.positive_button)).perform(click());

        // Wait for browsing data deletion to complete.
        mCallbackHelper.waitForOnly();

        assertDataTypesCleared(
                TimePeriod.LAST_HOUR,
                BrowsingDataType.HISTORY,
                BrowsingDataType.SITE_DATA,
                BrowsingDataType.CACHE);
    }

    @Test
    @MediumTest
    public void testQuickDeleteHistogram_WhenClickingCancel() {
        openQuickDeleteDialog();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        QuickDeleteMetricsDelegate.HISTOGRAM_NAME,
                        QuickDeleteMetricsDelegate.QuickDeleteAction.CANCEL_CLICKED);

        onViewWaiting(withId(R.id.negative_button)).perform(click());

        verify(mBrowsingDataBridgeMock, never())
                .clearBrowsingData(
                        any(), any(), any(), any(), anyInt(), any(), any(), any(), any());

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testQuickDeleteHistogram_WhenClickingMoreOptions() {
        openQuickDeleteDialog();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                QuickDeleteMetricsDelegate.HISTOGRAM_NAME,
                                QuickDeleteMetricsDelegate.QuickDeleteAction.MORE_OPTIONS_CLICKED)
                        .expectIntRecord(
                                QuickDeleteMetricsDelegate.HISTOGRAM_NAME,
                                QuickDeleteMetricsDelegate.QuickDeleteAction
                                        .DIALOG_DISMISSED_IMPLICITLY)
                        .build();

        onViewWaiting(withId(R.id.quick_delete_more_options)).perform(click());

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testQuickDeleteHistogram_WhenClickingBackButton() {
        openQuickDeleteDialog();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        QuickDeleteMetricsDelegate.HISTOGRAM_NAME,
                        QuickDeleteMetricsDelegate.QuickDeleteAction.DIALOG_DISMISSED_IMPLICITLY);

        // Implicitly dismiss pop up by pressing Clank's back button.
        pressBack();

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testMoreOptions_Triggers_ClearBrowsingData_Advanced() {
        final Instrumentation.ActivityMonitor activityMonitor =
                new Instrumentation.ActivityMonitor(SettingsActivity.class.getName(), null, false);
        InstrumentationRegistry.getInstrumentation().addMonitor(activityMonitor);

        openQuickDeleteDialog();
        // Wait for the dialog to show-up so we can retrieve the dialog in the next line.
        onViewWaiting(withId(R.id.quick_delete_spinner)).check(matches(isDisplayed()));
        onView(withId(R.id.quick_delete_more_options)).perform(click());

        SettingsActivity activity =
                (SettingsActivity)
                        InstrumentationRegistry.getInstrumentation()
                                .waitForMonitorWithTimeout(activityMonitor, ACTIVITY_WAIT_LONG_MS);

        assertTrue(activity.getMainFragment() instanceof ClearBrowsingDataFragmentAdvanced);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    public void testQuickDeleteTabsNotClosed_WithMultiInstance() {
        MultiWindowUtils.setInstanceCountForTesting(3);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher("Privacy.QuickDelete.TabsEnabled", false);

        mActivityTestRule.loadUrl("https://www.google.com/");
        assertEquals(1, mActivity.getCurrentTabModel().getCount());

        openQuickDeleteDialog();

        onViewWaiting(withId(R.id.positive_button)).perform(click());
        assertEquals(1, mActivity.getCurrentTabModel().getCount());
        histogramWatcher.assertExpected();
    }
}
