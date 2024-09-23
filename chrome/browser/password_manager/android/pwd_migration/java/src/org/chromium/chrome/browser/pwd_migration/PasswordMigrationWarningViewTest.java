// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ACCOUNT_DISPLAY_NAME;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.SHOULD_OFFER_SYNC;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.VISIBLE;

import android.view.View;

import androidx.test.espresso.Espresso;
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
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.MigrationOption;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ScreenType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link PasswordMigrationWarningView} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordMigrationWarningViewTest {

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock private Runnable mOnShowEventListener;
    @Mock private Callback<Integer> mDismissCallback;
    @Mock private PasswordMigrationWarningOnClickHandler mOnClickHandler;
    @Mock private PasswordMigrationWarningView.OnSheetClosedCallback mOnSheetClosedCallback;

    private BottomSheetController mBottomSheetController;
    private PasswordMigrationWarningView mView;
    private PropertyModel mModel;

    private static final String TEST_EMAIL = "user@domain.com";

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
                            PasswordMigrationWarningProperties.createDefaultModel(
                                    mOnShowEventListener, mDismissCallback, mOnClickHandler);
                    mView =
                            new PasswordMigrationWarningView(
                                    mActivityTestRule.getActivity(),
                                    mBottomSheetController,
                                    () -> {},
                                    (Throwable exception) -> fail(),
                                    mOnSheetClosedCallback);
                    PropertyModelChangeProcessor.create(
                            mModel,
                            mView,
                            PasswordMigrationWarningViewBinder::bindPasswordMigrationWarningView);
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
        verify(mOnSheetClosedCallback).onSheetClosed(StateChangeReason.NONE, false);
    }

    @Test
    @MediumTest
    public void testCallsOnShowListener() {
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        runOnUiThreadBlocking(() -> mModel.set(CURRENT_SCREEN, ScreenType.INTRO_SCREEN));
        // Wait for the fragment containing the button to be attached.
        pollUiThread(
                () ->
                        mActivityTestRule
                                        .getActivity()
                                        .findViewById(R.id.acknowledge_password_migration_button)
                                != null);
    }

    @Test
    @MediumTest
    public void testDismissesWhenHidden() {
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        // The sheet is hidden.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);

        // The dismiss callback was called.
        verify(mDismissCallback).onResult(BottomSheetController.StateChangeReason.NONE);
        verify(mOnSheetClosedCallback).onSheetClosed(StateChangeReason.NONE, false);
    }

    @Test
    @MediumTest
    public void testShowsIntroScreen() {
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        // Setting the introduction screen.
        runOnUiThreadBlocking(() -> mModel.set(CURRENT_SCREEN, ScreenType.INTRO_SCREEN));
        // The test waits for the fragment containing the button to be attached.
        pollUiThread(
                () ->
                        mActivityTestRule
                                        .getActivity()
                                        .findViewById(R.id.acknowledge_password_migration_button)
                                != null);
        onView(withId(R.id.migration_warning_sheet_subtitle)).check(matches(isDisplayed()));
        onView(withId(R.id.acknowledge_password_migration_button)).check(matches(isDisplayed()));
        onView(withId(R.id.password_migration_more_options_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testShowsOptionsScreen() {
        // Customize the view to display the sync option.
        runOnUiThreadBlocking(() -> mModel.set(SHOULD_OFFER_SYNC, true));
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        // Setting the options screen.
        runOnUiThreadBlocking(() -> mModel.set(CURRENT_SCREEN, ScreenType.OPTIONS_SCREEN));
        // The test waits for the fragment containing the button to be attached.
        pollUiThread(
                () ->
                        mActivityTestRule
                                        .getActivity()
                                        .findViewById(R.id.password_migration_cancel_button)
                                != null);
        onView(withId(R.id.radio_button_layout)).check(matches(isDisplayed()));
        runOnUiThreadBlocking(
                () -> {
                    RadioButtonWithDescription signInOrSyncButton =
                            mActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.radio_sign_in_or_sync);
                    assertTrue(signInOrSyncButton.isChecked());
                });
        onView(withId(R.id.password_migration_next_button)).check(matches(isDisplayed()));
        onView(withId(R.id.password_migration_cancel_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testShowsOptionsScreenNoSync() {
        // Customize the view to not display the sync option.
        runOnUiThreadBlocking(() -> mModel.set(SHOULD_OFFER_SYNC, false));
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        // Setting the options screen.
        runOnUiThreadBlocking(() -> mModel.set(CURRENT_SCREEN, ScreenType.OPTIONS_SCREEN));
        // The test waits for the fragment containing the button to be attached.
        pollUiThread(
                () ->
                        mActivityTestRule
                                        .getActivity()
                                        .findViewById(R.id.password_migration_cancel_button)
                                != null);
        onView(withId(R.id.radio_button_layout)).check(matches(isDisplayed()));
        runOnUiThreadBlocking(
                () -> {
                    RadioButtonWithDescription signInOrSyncButton =
                            mActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.radio_sign_in_or_sync);
                    assertEquals(View.GONE, signInOrSyncButton.getVisibility());
                });
        runOnUiThreadBlocking(
                () -> {
                    RadioButtonWithDescription exportButton =
                            mActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.radio_password_export);
                    assertTrue(exportButton.isChecked());
                });
        onView(withId(R.id.password_migration_next_button)).check(matches(isDisplayed()));
        onView(withId(R.id.password_migration_cancel_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testNextButtonPropagatesSyncOption() {
        // Customize the view to display the sync option.
        runOnUiThreadBlocking(() -> mModel.set(SHOULD_OFFER_SYNC, true));
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        // Setting the options screen.
        runOnUiThreadBlocking(() -> mModel.set(CURRENT_SCREEN, ScreenType.OPTIONS_SCREEN));
        // The test waits for the fragment containing the button to be attached.
        pollUiThread(
                () ->
                        mActivityTestRule
                                        .getActivity()
                                        .findViewById(R.id.password_migration_cancel_button)
                                != null);
        onView(withId(R.id.radio_button_layout)).check(matches(isDisplayed()));

        // Verify that the sync button is checked by default.
        runOnUiThreadBlocking(
                () -> {
                    RadioButtonWithDescription signInOrSyncButton =
                            mActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.radio_sign_in_or_sync);
                    assertTrue(signInOrSyncButton.isChecked());
                });

        onView(withId(R.id.password_migration_next_button)).perform(click());
        verify(mOnClickHandler)
                .onNext(
                        eq(MigrationOption.SYNC_PASSWORDS),
                        eq(mActivityTestRule.getActivity().getSupportFragmentManager()));
    }

    @Test
    @MediumTest
    public void testNextButtonPropagatesExportOption() {
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        // Setting the options screen.
        runOnUiThreadBlocking(() -> mModel.set(CURRENT_SCREEN, ScreenType.OPTIONS_SCREEN));
        // The test waits for the fragment containing the button to be attached.
        pollUiThread(
                () ->
                        mActivityTestRule
                                        .getActivity()
                                        .findViewById(R.id.password_migration_cancel_button)
                                != null);
        onView(withId(R.id.radio_button_layout)).check(matches(isDisplayed()));

        // Select the export button.
        runOnUiThreadBlocking(
                () -> {
                    RadioButtonWithDescription exportButton =
                            mActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.radio_password_export);
                    exportButton.setChecked(true);
                });

        onView(withId(R.id.password_migration_next_button)).perform(click());
        verify(mOnClickHandler)
                .onNext(
                        eq(MigrationOption.EXPORT_AND_DELETE),
                        eq(mActivityTestRule.getActivity().getSupportFragmentManager()));
    }

    /**
     * Checks that no crash happens and everything works as expected if CURRENT_SCREEN will be set
     * first. It can happen in production, because the order is not guaranteed.
     */
    @Test
    @MediumTest
    public void testCurrentScreenChangedBeforeVisibility() {
        // Setting the introduction screen.
        runOnUiThreadBlocking(() -> mModel.set(CURRENT_SCREEN, ScreenType.INTRO_SCREEN));
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        pollUiThread(
                () ->
                        mActivityTestRule
                                        .getActivity()
                                        .findViewById(R.id.acknowledge_password_migration_button)
                                != null);
        onView(withId(R.id.migration_warning_sheet_subtitle)).check(matches(isDisplayed()));
        onView(withId(R.id.acknowledge_password_migration_button)).check(matches(isDisplayed()));
        onView(withId(R.id.password_migration_more_options_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testAccountNameIsSet() {
        // Customize the view to display the sync option.
        runOnUiThreadBlocking(() -> mModel.set(SHOULD_OFFER_SYNC, true));
        // Setting the options screen.
        runOnUiThreadBlocking(() -> mModel.set(CURRENT_SCREEN, ScreenType.OPTIONS_SCREEN));
        // Setting the profile.
        runOnUiThreadBlocking(() -> mModel.set(ACCOUNT_DISPLAY_NAME, TEST_EMAIL));
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        pollUiThread(
                () ->
                        mActivityTestRule
                                        .getActivity()
                                        .findViewById(R.id.password_migration_next_button)
                                != null);
        onView(withText(TEST_EMAIL)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testEmptySheetClosedWithoutUserInteractionCallsOnSheetClosedCallback() {
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        BottomSheetTestSupport.waitForState(mBottomSheetController, SheetState.HIDDEN);

        verify(mOnSheetClosedCallback).onSheetClosed(StateChangeReason.NONE, false);
    }

    @Test
    @MediumTest
    public void testEmptySheetClosedByUserInteractionCallsOnSheetClosedCallback() {
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        Espresso.pressBack();
        BottomSheetTestSupport.waitForState(mBottomSheetController, SheetState.HIDDEN);

        verify(mOnSheetClosedCallback).onSheetClosed(StateChangeReason.BACK_PRESS, false);
    }

    @Test
    @MediumTest
    public void testClosingTheSheetWithFullContentCallsOnSheetClosedCallback() {
        runOnUiThreadBlocking(() -> mModel.set(CURRENT_SCREEN, ScreenType.INTRO_SCREEN));
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        pollUiThread(
                () ->
                        mActivityTestRule
                                        .getActivity()
                                        .findViewById(R.id.acknowledge_password_migration_button)
                                != null);
        Espresso.pressBack();
        BottomSheetTestSupport.waitForState(mBottomSheetController, SheetState.HIDDEN);

        verify(mOnSheetClosedCallback).onSheetClosed(StateChangeReason.BACK_PRESS, true);
    }

    private @SheetState int getBottomSheetState() {
        return mBottomSheetController.getSheetState();
    }
}
