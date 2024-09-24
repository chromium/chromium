// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING;
import static org.chromium.chrome.browser.password_manager.PasswordMetricsUtil.POST_PASSWORD_MIGRATION_SHEET_OUTCOME;
import static org.chromium.chrome.browser.pwd_migration.PostPasswordMigrationSheetProperties.VISIBLE;

import android.content.Context;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordMetricsUtil.PostPasswordMigrationSheetOutcome;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link PostPasswordMigrationSheetView} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PostPasswordMigrationSheetViewTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock private Callback<Integer> mDismissCallback;

    private BottomSheetController mBottomSheetController;
    private PostPasswordMigrationSheetView mView;
    private PropertyModel mModel;

    @Before
    public void setupTest() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mBottomSheetController =
                mActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController();
        runOnUiThreadBlocking(
                () -> {
                    mModel =
                            PostPasswordMigrationSheetProperties.createDefaultModel(
                                    mDismissCallback);
                    mView =
                            new PostPasswordMigrationSheetView(
                                    mActivityTestRule.getActivity(), mBottomSheetController);
                    PostPasswordMigrationSheetCoordinator.setUpModelChangeProcessors(mModel, mView);
                });
    }

    @Test
    @MediumTest
    public void testVisibilityChangedByModel() {
        // After setting the visibility to true, the view should exist and be visible.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        assertThat(mView.getContentView().isShown(), is(true));

        // After hiding the view, the view should still exist but be invisible.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
        assertThat(mView.getContentView().isShown(), is(false));
    }

    @Test
    @MediumTest
    public void testDismissCalledWhenHidden() {
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        // The sheet is hidden.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);

        // The dismiss callback was called.
        verify(mDismissCallback).onResult(StateChangeReason.NAVIGATION);
    }

    @Test
    @MediumTest
    public void testAcknowledgingTheNoticeClosesTheSheet() {
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // The notice is acknowledged.
        onView(withId(R.id.acknowledge_button)).perform(click());

        // The dismiss callback was called.
        verify(mDismissCallback).onResult(StateChangeReason.NAVIGATION);
    }

    @Test
    @MediumTest
    public void testAcceptingTheNoticeRecordsMetrics() {
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                POST_PASSWORD_MIGRATION_SHEET_OUTCOME,
                                PostPasswordMigrationSheetOutcome.GOT_IT)
                        .build();

        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // The notice is acknowledged.
        onView(withId(R.id.acknowledge_button)).perform(click());

        histogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testDismissingTheNoticeRecordsMetrics() {
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                POST_PASSWORD_MIGRATION_SHEET_OUTCOME,
                                PostPasswordMigrationSheetOutcome.DISMISS)
                        .build();

        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // The notice is dismissed.
        pressBack();

        histogram.assertExpected();
    }

    @Test
    @MediumTest
    @DisableFeatures(UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)
    public void sheetSetsTheTitleAndSubtitle() {
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        Context context = mActivityTestRule.getActivity();
        onView(withText(context.getString(R.string.post_password_migration_sheet_title)))
                .check(matches(isDisplayed()));
        onView(
                        withText(
                                context.getString(R.string.post_password_migration_sheet_subtitle)
                                        .replace("%1$s", "")))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures(UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)
    @DisabledTest(message = "crbug.com/369371078")
    public void sheetSetsTheTitleAndSubtitleAboutLocalPasswords() {
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        Context context = mActivityTestRule.getActivity();
        onView(
                        withText(
                                context.getString(
                                        R.string
                                                .post_password_migration_sheet_title_about_local_pwd)))
                .check(matches(isDisplayed()));
        onView(
                        withText(
                                context.getString(
                                                R.string
                                                        .post_pwd_migration_sheet_subtitle_about_local_pwd)
                                        .replace("%1$s", "")))
                .check(matches(isDisplayed()));
    }

    private @SheetState int getBottomSheetState() {
        return mBottomSheetController.getSheetState();
    }
}
