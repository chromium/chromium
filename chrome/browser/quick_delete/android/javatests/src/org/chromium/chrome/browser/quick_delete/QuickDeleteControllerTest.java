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
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;

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

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
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
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.io.IOException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests for quick delete controller.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.QUICK_DELETE_FOR_ANDROID)
@Batch(Batch.PER_CLASS)
public class QuickDeleteControllerTest {
    private static final String TEST_FILE = "/content/test/data/browsing_data/site_data.html";
    private static final long ACTIVITY_WAIT_LONG_MS = TimeUnit.SECONDS.toMillis(10);
    private static final long FIFTEEN_MINUTES_IN_MS = TimeUnit.MINUTES.toMillis(15);

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.PRIVACY)
                    .build();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private BrowsingDataBridge.Natives mBrowsingDataBridgeMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();

        // Set the time for the initial tab to outside of the quick delete timespan.
        Tab initialTab = mActivityTestRule.getActivity().getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ((TabImpl) initialTab)
                    .setLastNavigationCommittedTimestampMillis(
                            System.currentTimeMillis() - FIFTEEN_MINUTES_IN_MS);
        });

        // Open new tab for tests.
        mActivityTestRule.loadUrlInNewTab("about:blank");
    }

    @After
    public void tearDown() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabModel model = activity.getTabModelSelector().getModel(false);
            // Close all tabs for the next test.
            model.closeAllTabs();
        });
    }

    private void openQuickDeleteDialog() {
        // Open 3 dot menu.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });
        onViewWaiting(withId(R.id.app_menu_list))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        // Click on quick delete menu item.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.callOnItemClick(
                    mActivityTestRule.getAppMenuCoordinator(), R.id.quick_delete_menu_id);
        });
        onViewWaiting(withId(R.id.quick_delete_spinner))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
    }

    private void resetCookies() throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingData(helper::notifyCalled,
                    new int[] {BrowsingDataType.COOKIES}, TimePeriod.LAST_15_MINUTES);
        });
        helper.waitForCallback(0);
    }

    private void loadSiteDataUrl() {
        String url = mActivityTestRule.getTestServer().getURL(TEST_FILE);
        mActivityTestRule.loadUrlInNewTab(url);
    }

    private String runJavascriptSync(String type) throws Exception {
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mActivityTestRule.getWebContents(), type);
    }

    @Test
    @MediumTest
    public void testNavigateToTabSwitcher_WhenClickingDelete() throws IOException {
        openQuickDeleteDialog();
        onViewWaiting(withId(R.id.positive_button)).perform(click());

        LayoutTestUtils.waitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testSnackbarShown_WhenClickingDelete() throws IOException {
        openQuickDeleteDialog();
        onViewWaiting(withId(R.id.positive_button)).perform(click());

        onViewWaiting(withId(R.id.snackbar)).check(matches(isDisplayed()));
        onView(withText(mActivityTestRule.getActivity().getString(
                       R.string.quick_delete_snackbar_message,
                       TimePeriodUtils.getTimePeriodString(
                               mActivityTestRule.getActivity(), TimePeriod.LAST_15_MINUTES))))
                .check(matches(isDisplayed()));

        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(R.id.snackbar),
                "quick_delete_snackbar");
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1477790")
    public void testSnackbarShown_WhenClickingDelete_AllTimeSelected() {
        openQuickDeleteDialog();
        onView(withId(R.id.quick_delete_spinner)).check(matches(isDisplayed()));
        View dialogView = mActivityTestRule.getActivity()
                                  .getModalDialogManager()
                                  .getCurrentDialogForTest()
                                  .get(ModalDialogProperties.CUSTOM_VIEW);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Spinner spinnerView = dialogView.findViewById(R.id.quick_delete_spinner);
            spinnerView.setSelection(5);
            var option = (TimePeriodUtils.TimePeriodSpinnerOption) spinnerView.getSelectedItem();
            assertEquals(TimePeriod.ALL_TIME, option.getTimePeriod());
        });

        onViewWaiting(withId(R.id.positive_button)).perform(click());
        onViewWaiting(withId(R.id.snackbar)).check(matches(isDisplayed()));
        onView(withText(R.string.quick_delete_snackbar_all_time_message))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testDeleteClickedHistogram_WhenClickingDelete() throws IOException {
        openQuickDeleteDialog();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(QuickDeleteMetricsDelegate.HISTOGRAM_NAME,
                        QuickDeleteMetricsDelegate.QuickDeleteAction.DELETE_CLICKED);

        onViewWaiting(withId(R.id.positive_button)).perform(click());

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testQuickDeleteHistogram_WhenClickingDelete() throws IOException {
        openQuickDeleteDialog();

        HistogramWatcher histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                "Privacy.DeleteBrowsingData.Action", DeleteBrowsingDataAction.QUICK_DELETE);

        onViewWaiting(withId(R.id.positive_button)).perform(click());

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testQuickDeleteTimePeriodToggle_DeletesCorrectRange() {
        openQuickDeleteDialog();
        // Wait for the dialog to show-up so we can retrieve the dialog in the next line.
        onViewWaiting(withId(R.id.quick_delete_spinner)).check(matches(isDisplayed()));

        View dialogView = mActivityTestRule.getActivity()
                                  .getModalDialogManager()
                                  .getCurrentDialogForTest()
                                  .get(ModalDialogProperties.CUSTOM_VIEW);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Spinner spinnerView = dialogView.findViewById(R.id.quick_delete_spinner);
            // Set the time selection for LAST_HOUR.
            spinnerView.setSelection(1);
            var option = (TimePeriodUtils.TimePeriodSpinnerOption) spinnerView.getSelectedItem();
            assertEquals(TimePeriod.LAST_HOUR, option.getTimePeriod());

            // Check below that the browsing data bridge is called for LAST_HOUR.
            mJniMocker.mock(BrowsingDataBridgeJni.TEST_HOOKS, mBrowsingDataBridgeMock);
            doNothing()
                    .when(mBrowsingDataBridgeMock)
                    .clearBrowsingData(any(), any(), any(), eq(TimePeriod.LAST_HOUR), any(), any(),
                            any(), any());
        });

        onViewWaiting(withId(R.id.positive_button)).perform(click());
        verify(mBrowsingDataBridgeMock)
                .clearBrowsingData(
                        any(), any(), any(), eq(TimePeriod.LAST_HOUR), any(), any(), any(), any());
    }

    @Test
    @MediumTest
    public void testCancelClickedHistogram_WhenClickingCancel() throws IOException {
        openQuickDeleteDialog();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(QuickDeleteMetricsDelegate.HISTOGRAM_NAME,
                        QuickDeleteMetricsDelegate.QuickDeleteAction.CANCEL_CLICKED);

        onViewWaiting(withId(R.id.negative_button)).perform(click());

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testMoreOptionsClickedHistogram_WhenClickingMoreOptions() throws IOException {
        openQuickDeleteDialog();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(QuickDeleteMetricsDelegate.HISTOGRAM_NAME,
                                QuickDeleteMetricsDelegate.QuickDeleteAction.MORE_OPTIONS_CLICKED)
                        .expectIntRecord(QuickDeleteMetricsDelegate.HISTOGRAM_NAME,
                                QuickDeleteMetricsDelegate.QuickDeleteAction
                                        .DIALOG_DISMISSED_IMPLICITLY)
                        .build();

        onViewWaiting(withId(R.id.quick_delete_more_options)).perform(click());

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testDialogDismissedImplicitlyHistogram_WhenClickingBackButton() throws IOException {
        openQuickDeleteDialog();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(QuickDeleteMetricsDelegate.HISTOGRAM_NAME,
                        QuickDeleteMetricsDelegate.QuickDeleteAction.DIALOG_DISMISSED_IMPLICITLY);

        // Implicitly dismiss pop up by pressing Clank's back button.
        pressBack();

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testBrowsingDataDeletion_onClickedDelete() throws Exception {
        resetCookies();
        loadSiteDataUrl();
        assertEquals("false", runJavascriptSync("hasCookie()"));

        runJavascriptSync("setCookie()");
        assertEquals("true", runJavascriptSync("hasCookie()"));

        // Browsing data (cookies) should be deleted.
        openQuickDeleteDialog();
        onViewWaiting(withId(R.id.positive_button)).perform(click());
        onView(withId(R.id.snackbar)).check(matches(isDisplayed()));
        // Since the previous tab was deleted and we are in the tab switcher, we need to open a new
        // tab.
        loadSiteDataUrl();
        assertEquals("false", runJavascriptSync("hasCookie()"));
    }

    @Test
    @MediumTest
    public void testBrowsingDataDeletion_onClickedCancel() throws Exception {
        resetCookies();
        loadSiteDataUrl();
        assertEquals("false", runJavascriptSync("hasCookie()"));

        runJavascriptSync("setCookie()");
        assertEquals("true", runJavascriptSync("hasCookie()"));

        // Browsing data (cookies) should not be deleted.
        openQuickDeleteDialog();
        onViewWaiting(withId(R.id.negative_button)).perform(click());
        assertEquals("true", runJavascriptSync("hasCookie()"));
    }

    @Test
    @MediumTest
    public void testMoreOptions_Triggers_ClearBrowsingData_Advanced() throws IOException {
        final Instrumentation.ActivityMonitor activityMonitor =
                new Instrumentation.ActivityMonitor(SettingsActivity.class.getName(), null, false);
        InstrumentationRegistry.getInstrumentation().addMonitor(activityMonitor);

        openQuickDeleteDialog();
        // Wait for the dialog to show-up so we can retrieve the dialog in the next line.
        onViewWaiting(withId(R.id.quick_delete_spinner)).check(matches(isDisplayed()));
        onView(withId(R.id.quick_delete_more_options)).perform(click());

        SettingsActivity activity =
                (SettingsActivity) InstrumentationRegistry.getInstrumentation()
                        .waitForMonitorWithTimeout(activityMonitor, ACTIVITY_WAIT_LONG_MS);

        assertTrue(activity.getMainFragment() instanceof ClearBrowsingDataFragmentAdvanced);
    }
}
