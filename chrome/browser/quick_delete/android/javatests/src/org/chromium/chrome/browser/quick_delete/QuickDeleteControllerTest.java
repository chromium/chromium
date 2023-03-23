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

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.IOException;

/**
 * Tests for quick delete controller.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.QUICK_DELETE_FOR_ANDROID})
@Batch(Batch.PER_CLASS)
public class QuickDeleteControllerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.PRIVACY)
                    .build();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
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

        onView(withId(R.id.snackbar)).check(matches(isDisplayed()));
        onView(withText(R.string.quick_delete_snackbar_message)).check(matches(isDisplayed()));

        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(R.id.snackbar),
                "quick_delete_snackbar");
    }

    @Test
    @MediumTest
    public void testDeleteClickedHistogram_WhenClickingDelete() throws IOException {
        openQuickDeleteDialog();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Privacy.QuickDelete",
                                QuickDeleteMetricsDelegate.PrivacyQuickDelete.DELETE_CLICKED, 1)
                        .build();

        onViewWaiting(withId(R.id.positive_button)).perform(click());

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testQuickDeleteLast15MinutesHistogram_WhenClickingDelete() throws IOException {
        openQuickDeleteDialog();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Privacy.ClearBrowsingData.Action",
                                ClearBrowsingDataAction.QUICK_DELETE_LAST15_MINUTES, 1)
                        .build();

        onViewWaiting(withId(R.id.positive_button)).perform(click());

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testCancelClickedHistogram_WhenClickingCancel() throws IOException {
        openQuickDeleteDialog();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Privacy.QuickDelete",
                                QuickDeleteMetricsDelegate.PrivacyQuickDelete.CANCEL_CLICKED, 1)
                        .build();

        onViewWaiting(withId(R.id.negative_button)).perform(click());

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testDialogDismissedImplicitlyHistogram_WhenClickingBackButton() throws IOException {
        openQuickDeleteDialog();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Privacy.QuickDelete",
                                QuickDeleteMetricsDelegate.PrivacyQuickDelete
                                        .DIALOG_DISMISSED_IMPLICITLY,
                                1)
                        .build();

        // Implicitly dismiss pop up by pressing Clank's back button.
        pressBack();

        histogramWatcher.assertExpected();
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
    }
}
