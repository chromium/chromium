// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.core.IsNot.not;
import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;
import android.widget.Spinner;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.CollectionUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.browsing_data.TimePeriodUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for quick delete dialog view. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.QUICK_DELETE_FOR_ANDROID)
@Batch(Batch.PER_CLASS)
public class QuickDeleteDialogDelegateTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.PRIVACY)
                    .build();

    @Mock private SyncService mMockSyncService;

    private ChromeTabbedActivity mActivity;

    @Before
    public void setUp() {
        initMocks(this);
        SyncServiceFactory.setInstanceForTesting(mMockSyncService);
        setSyncable(false);

        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
    }

    @After
    public void tearDown() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();

        // Close all tabs
        runOnUiThreadBlocking(
                () ->
                        mActivity
                                .getCurrentTabModel()
                                .closeTabs(TabClosureParams.closeAllTabs().build()));

        // Clear history.
        runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getForProfile(ProfileManager.getLastUsedRegularProfile())
                            .clearBrowsingData(
                                    callbackHelper::notifyCalled,
                                    new int[] {BrowsingDataType.HISTORY},
                                    TimePeriod.ALL_TIME);
                });

        callbackHelper.waitForOnly();
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
    }

    private void setSyncable(boolean syncable) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(mMockSyncService.getActiveDataTypes())
                            .thenReturn(
                                    syncable
                                            ? CollectionUtil.newHashSet(
                                                    DataType.HISTORY_DELETE_DIRECTIVES)
                                            : new HashSet<>());
                });
    }

    private List<Tab> getTabsInCurrentTabModel() {
        List<Tab> tabs = new ArrayList<>();

        TabModel currentTabModel = mActivity.getCurrentTabModel();
        for (int i = 0; i < currentTabModel.getCount(); i++) {
            tabs.add(currentTabModel.getTabAt(i));
        }

        return tabs;
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Feature({"RenderTest"})
    public void testQuickDeleteDialogView_WithSignInAndSync() throws IOException {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        setSyncable(true);

        mActivityTestRule.loadUrl("https://www.example.com/");
        mActivityTestRule.loadUrlInNewTab("https://www.google.com/");
        assertEquals(2, getTabsInCurrentTabModel().size());

        openQuickDeleteDialog();

        onView(withText(R.string.quick_delete_dialog_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withId(R.id.quick_delete_spinner)).check(matches(isDisplayed()));
        onView(withId(R.id.quick_delete_history_row)).check(matches(isDisplayed()));
        onViewWaiting(withText(R.string.quick_delete_dialog_browsing_history_secondary_text))
                .check(matches(isDisplayed()));
        onView(
                        withText(
                                mActivity
                                        .getResources()
                                        .getQuantityString(
                                                R.plurals.quick_delete_dialog_tabs_closed_text,
                                                2,
                                                2)))
                .check(matches(isDisplayed()));
        onView(withText(R.string.quick_delete_dialog_cookies_cache_and_other_site_data_text))
                .check(matches(isDisplayed()));
        onView(withId(R.id.search_history_disambiguation)).check(matches(isDisplayed()));
        onView(withId(R.id.quick_delete_more_options)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button))
                .check(matches(withText(mActivity.getString(R.string.clear_data_delete))));
        onView(withId(R.id.negative_button))
                .check(matches(withText(mActivity.getString(R.string.cancel))));

        View dialogView =
                mActivity
                        .getModalDialogManager()
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CUSTOM_VIEW);
        mRenderTestRule.render(dialogView, "quick_delete_dialog-signed-in-and-sync");
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Feature({"RenderTest"})
    public void testQuickDeleteDialogView_WithSignInOnly() throws IOException {
        mSigninTestRule.addTestAccountThenSignin();
        setSyncable(false);

        mActivityTestRule.loadUrl("https://www.google.com/");
        assertEquals(1, getTabsInCurrentTabModel().size());

        openQuickDeleteDialog();

        onView(withText(R.string.quick_delete_dialog_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withId(R.id.quick_delete_spinner)).check(matches(isDisplayed()));
        onView(withId(R.id.quick_delete_history_row)).check(matches(isDisplayed()));
        onView(withText(R.string.quick_delete_dialog_browsing_history_secondary_text))
                .check(matches(not(isDisplayed())));
        onView(
                        withText(
                                mActivity
                                        .getResources()
                                        .getQuantityString(
                                                R.plurals.quick_delete_dialog_tabs_closed_text, 1)))
                .check(matches(isDisplayed()));
        onView(withText(R.string.quick_delete_dialog_cookies_cache_and_other_site_data_text))
                .check(matches(isDisplayed()));
        onView(withId(R.id.search_history_disambiguation)).check(matches(isDisplayed()));
        onView(withId(R.id.quick_delete_more_options)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button))
                .check(matches(withText(mActivity.getString(R.string.clear_data_delete))));
        onView(withId(R.id.negative_button))
                .check(matches(withText(mActivity.getString(R.string.cancel))));

        View dialogView =
                mActivity
                        .getModalDialogManager()
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CUSTOM_VIEW);
        mRenderTestRule.render(dialogView, "quick_delete_dialog-signed-in");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testQuickDeleteDialogView_WithoutTabsOrHistory() throws IOException {
        String timePeriodString = mActivity.getString(R.string.quick_delete_time_period_15_minutes);

        runOnUiThreadBlocking(
                () ->
                        mActivity
                                .getCurrentTabModel()
                                .closeTabs(TabClosureParams.closeAllTabs().build()));
        assertEquals(0, getTabsInCurrentTabModel().size());
        LayoutTestUtils.waitForLayout(mActivity.getLayoutManager(), LayoutType.TAB_SWITCHER);

        openQuickDeleteDialog();

        onView(withText(R.string.quick_delete_dialog_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withId(R.id.quick_delete_spinner)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(
                        withText(
                                mActivity.getString(
                                        R.string
                                                .quick_delete_dialog_zero_browsing_history_domain_count_text,
                                        timePeriodString)))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(
                        withText(
                                mActivity.getString(
                                        R.string.quick_delete_dialog_zero_tabs_closed_text,
                                        timePeriodString)))
                .check(matches(isDisplayed()));
        onView(withText(R.string.quick_delete_dialog_cookies_cache_and_other_site_data_text))
                .check(matches(isDisplayed()));
        onView(withId(R.id.search_history_disambiguation)).check(matches(not(isDisplayed())));
        onView(withId(R.id.quick_delete_more_options)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button))
                .check(matches(withText(mActivity.getString(R.string.clear_data_delete))));
        onView(withId(R.id.negative_button))
                .check(matches(withText(mActivity.getString(R.string.cancel))));

        View dialogView =
                mActivity
                        .getModalDialogManager()
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CUSTOM_VIEW);
        mRenderTestRule.render(dialogView, "quick_delete_dialog-no-tabs-or-history");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testQuickDeleteDialogView_MultiInstance() throws IOException {
        MultiWindowUtils.setInstanceCountForTesting(3);
        openQuickDeleteDialog();

        onView(withId(R.id.quick_delete_tabs_row_subtitle)).check(matches(isDisplayed()));

        View dialogView =
                mActivity
                        .getModalDialogManager()
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CUSTOM_VIEW);
        mRenderTestRule.render(dialogView, "quick_delete_dialog-tabs-disabled");
    }

    @Test
    @MediumTest
    public void testQuickDeleteDialogSpinnerViewContents() {
        openQuickDeleteDialog();
        onView(withId(R.id.quick_delete_spinner)).inRoot(isDialog()).check(matches(isDisplayed()));
        View dialogView =
                mActivity
                        .getModalDialogManager()
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CUSTOM_VIEW);
        Spinner spinnerView = dialogView.findViewById(R.id.quick_delete_spinner);
        assertEquals(6, spinnerView.getAdapter().getCount());
        assertEquals(
                TimePeriod.LAST_15_MINUTES,
                ((TimePeriodUtils.TimePeriodSpinnerOption) spinnerView.getSelectedItem())
                        .getTimePeriod());
        assertEquals(
                mActivity.getString(R.string.clear_browsing_data_tab_period_15_minutes),
                spinnerView.getItemAtPosition(0).toString());
        assertEquals(
                mActivity.getString(R.string.clear_browsing_data_tab_period_hour),
                spinnerView.getItemAtPosition(1).toString());
        assertEquals(
                mActivity.getString(R.string.clear_browsing_data_tab_period_24_hours),
                spinnerView.getItemAtPosition(2).toString());
        assertEquals(
                mActivity.getString(R.string.clear_browsing_data_tab_period_7_days),
                spinnerView.getItemAtPosition(3).toString());
        assertEquals(
                mActivity.getString(R.string.clear_browsing_data_tab_period_four_weeks),
                spinnerView.getItemAtPosition(4).toString());
        assertEquals(
                mActivity.getString(R.string.clear_browsing_data_tab_period_everything),
                spinnerView.getItemAtPosition(5).toString());
    }
}
