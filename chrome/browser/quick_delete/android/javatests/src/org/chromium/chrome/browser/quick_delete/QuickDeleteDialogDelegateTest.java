// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.core.IsNot.not;
import static org.junit.Assert.assertEquals;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;
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

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.widget.ButtonCompat;

import java.io.IOException;
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

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();

        // Clear history.
        runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getInstance()
                            .clearBrowsingData(
                                    callbackHelper::notifyCalled,
                                    new int[] {BrowsingDataType.HISTORY},
                                    TimePeriod.ALL_TIME);
                });

        callbackHelper.waitForCallback(0);

        mSigninTestRule.forceSignOut();
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

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1459455")
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Feature({"RenderTest"})
    public void testQuickDeleteDialogView_WithSignInAndSync() throws IOException {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        mActivityTestRule.loadUrl("https://www.example.com/");
        mActivityTestRule.loadUrl("https://www.google.com/");

        openQuickDeleteDialog();

        onView(withText(R.string.quick_delete_dialog_title)).check(matches(isDisplayed()));
        onView(withId(R.id.quick_delete_spinner)).check(matches(isDisplayed()));
        onView(withId(R.id.quick_delete_history_row)).check(matches(isDisplayed()));
        onView(withText(R.string.quick_delete_dialog_browsing_history_secondary_text))
                .check(matches(isDisplayed()));
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getResources()
                                        .getQuantityString(
                                                R.plurals.quick_delete_dialog_tabs_closed_text, 1)))
                .check(matches(isDisplayed()));
        onView(withText(R.string.quick_delete_dialog_cookies_cache_and_other_site_data_text))
                .check(matches(isDisplayed()));
        onView(withId(R.id.search_history_disambiguation)).check(matches(isDisplayed()));
        onView(withId(R.id.quick_delete_more_options)).check(matches(isDisplayed()));

        // TODO(crbug.com/1412087): Get the full dialog for render test instead of just the custom
        // view.
        View dialogView =
                mActivityTestRule
                        .getActivity()
                        .getModalDialogManager()
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CUSTOM_VIEW);
        mRenderTestRule.render(dialogView, "quick_delete_dialog-signed-in-and-sync");
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1459604")
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Feature({"RenderTest"})
    public void testQuickDeleteDialogView_WithSignInOnly() throws IOException {
        mSigninTestRule.addTestAccountThenSignin();
        mActivityTestRule.loadUrl("https://www.google.com/");

        openQuickDeleteDialog();

        onView(withText(R.string.quick_delete_dialog_title)).check(matches(isDisplayed()));
        onView(withId(R.id.quick_delete_spinner)).check(matches(isDisplayed()));
        onView(withId(R.id.quick_delete_history_row)).check(matches(isDisplayed()));
        onView(withText(R.string.quick_delete_dialog_browsing_history_secondary_text))
                .check(matches(not(isDisplayed())));
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getResources()
                                        .getQuantityString(
                                                R.plurals.quick_delete_dialog_tabs_closed_text, 1)))
                .check(matches(isDisplayed()));
        onView(withText(R.string.quick_delete_dialog_cookies_cache_and_other_site_data_text))
                .check(matches(isDisplayed()));
        onView(withId(R.id.search_history_disambiguation)).check(matches(isDisplayed()));
        onView(withId(R.id.quick_delete_more_options)).check(matches(isDisplayed()));

        View dialogView =
                mActivityTestRule
                        .getActivity()
                        .getModalDialogManager()
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CUSTOM_VIEW);
        mRenderTestRule.render(dialogView, "quick_delete_dialog-signed-in");
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1459604")
    @Feature({"RenderTest"})
    public void testQuickDeleteDialogView_WithoutTabsOrHistory() throws IOException {
        String timePeriodString =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.quick_delete_time_period_15_minutes);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getCurrentTabModel().closeAllTabs(false));

        openQuickDeleteDialog();

        onView(withText(R.string.quick_delete_dialog_title)).check(matches(isDisplayed()));
        onView(withId(R.id.quick_delete_spinner)).check(matches(isDisplayed()));
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(
                                                R.string
                                                        .quick_delete_dialog_zero_browsing_history_domain_count_text,
                                                timePeriodString)))
                .check(matches(isDisplayed()));
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(
                                                R.string.quick_delete_dialog_zero_tabs_closed_text,
                                                timePeriodString)))
                .check(matches(isDisplayed()));
        onView(withText(R.string.quick_delete_dialog_cookies_cache_and_other_site_data_text))
                .check(matches(isDisplayed()));
        onView(withId(R.id.search_history_disambiguation)).check(matches(not(isDisplayed())));
        onView(withId(R.id.quick_delete_more_options)).check(matches(isDisplayed()));

        View dialogView =
                mActivityTestRule
                        .getActivity()
                        .getModalDialogManager()
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CUSTOM_VIEW);
        mRenderTestRule.render(dialogView, "quick_delete_dialog-no-tabs-or-history");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testQuickDeleteDialogDefaultSpinnerView() throws IOException {
        openQuickDeleteDialog();
        onView(withId(R.id.quick_delete_spinner)).check(matches(isDisplayed()));
        View dialogView =
                mActivityTestRule
                        .getActivity()
                        .getModalDialogManager()
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CUSTOM_VIEW);
        Spinner spinnerView = dialogView.findViewById(R.id.quick_delete_spinner);
        mRenderTestRule.render(spinnerView, "quick_delete_dialog_spinner_default");
    }

    @Test
    @MediumTest
    public void testQuickDeleteDialogSpinnerViewContents() throws IOException {
        openQuickDeleteDialog();
        onView(withId(R.id.quick_delete_spinner)).check(matches(isDisplayed()));
        View dialogView =
                mActivityTestRule
                        .getActivity()
                        .getModalDialogManager()
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CUSTOM_VIEW);
        Spinner spinnerView = dialogView.findViewById(R.id.quick_delete_spinner);
        assertEquals(6, spinnerView.getAdapter().getCount());
        assertEquals(
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.clear_browsing_data_tab_period_15_minutes),
                spinnerView.getItemAtPosition(0).toString());
        assertEquals(
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.clear_browsing_data_tab_period_hour),
                spinnerView.getItemAtPosition(1).toString());
        assertEquals(
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.clear_browsing_data_tab_period_24_hours),
                spinnerView.getItemAtPosition(2).toString());
        assertEquals(
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.clear_browsing_data_tab_period_7_days),
                spinnerView.getItemAtPosition(3).toString());
        assertEquals(
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.clear_browsing_data_tab_period_four_weeks),
                spinnerView.getItemAtPosition(4).toString());
        assertEquals(
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.clear_browsing_data_tab_period_everything),
                spinnerView.getItemAtPosition(5).toString());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testQuickDeleteDialogMoreOptionsButton() throws IOException {
        openQuickDeleteDialog();
        onView(withId(R.id.quick_delete_more_options)).check(matches(isDisplayed()));
        View dialogView =
                mActivityTestRule
                        .getActivity()
                        .getModalDialogManager()
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CUSTOM_VIEW);
        ButtonCompat moreOptionsView = dialogView.findViewById(R.id.quick_delete_more_options);
        mRenderTestRule.render(moreOptionsView, "quick_delete_dialog_more-options");
    }
}
