// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.ILLUSTRATION_VISIBLE;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/** Test for the password manager illustration modal dialog. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordManagerDialogTest {
    private PasswordManagerDialogCoordinator mCoordinator;
    private PasswordManagerDialogMediator mMediator;
    private PropertyModel mModel;
    private static final String TITLE = "Title";
    private static final String DETAILS = "Explanation text.";
    private static final String OK_BUTTON = "OK";
    private static final String CANCEL_BUTTON = "Cancel";

    @Mock private Callback<Integer> mOnClick;

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeActivity activity = (ChromeActivity) sActivityTestRule.getActivity();
                    ModalDialogManager dialogManager = activity.getModalDialogManager();
                    mCoordinator =
                            new PasswordManagerDialogCoordinator(
                                    dialogManager,
                                    activity.findViewById(android.R.id.content),
                                    activity.getBrowserControlsManager());
                    PasswordManagerDialogContents contents =
                            new PasswordManagerDialogContents(
                                    TITLE,
                                    DETAILS,
                                    R.drawable.password_checkup_warning,
                                    OK_BUTTON,
                                    CANCEL_BUTTON,
                                    mOnClick);
                    contents.setDialogType(ModalDialogManager.ModalDialogType.TAB);
                    mCoordinator.initialize(
                            activity.getWindowAndroid().getContext().get(), contents);
                    mMediator = mCoordinator.getMediatorForTesting();
                    mModel = mMediator.getModelForTesting();
                    mCoordinator.showDialog();
                });
        onViewWaiting(withId(R.id.positive_button));
    }

    @Test
    @SmallTest
    public void testDialogSubviewsData() {
        onView(withId(R.id.password_manager_dialog_title)).check(matches(withText(TITLE)));
        onView(withId(R.id.password_manager_dialog_details)).check(matches(withText(DETAILS)));
        onView(withId(R.id.password_manager_dialog_illustration)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button)).check(matches(withText(OK_BUTTON)));
        onView(withId(R.id.negative_button)).check(matches(withText(CANCEL_BUTTON)));
    }

    @Test
    @SmallTest
    public void testAcceptedCallback() {
        onView(withId(R.id.positive_button)).perform(click());
        verify(mOnClick).onResult(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    @Test
    @SmallTest
    public void testRejectedCallback() {
        onView(withId(R.id.negative_button)).perform(click());
        verify(mOnClick).onResult(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Test
    @SmallTest
    public void testDismissedCallbackBackButton() {
        pressBack();
        verify(mOnClick).onResult(DialogDismissalCause.NAVIGATE_BACK);
    }

    @Test
    @SmallTest
    public void testSettingImageVisibility() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ILLUSTRATION_VISIBLE, false);
                });
        onView(withId(R.id.password_manager_dialog_illustration))
                .check(matches(not(isDisplayed())));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ILLUSTRATION_VISIBLE, true);
                });
        onView(withId(R.id.password_manager_dialog_illustration)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testWatchingLayoutChanges() {
        float dipScale =
                sActivityTestRule.getActivity().getWindowAndroid().getDisplay().getDipScale();

        // Dimensions resembling landscape orientation.
        final int testHeightDipLandscape = 300; // Height of the android content view.
        final int testWidthDipLandscape = 500; // Width of the android content view.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMediator.onLayoutChange(
                            null,
                            0,
                            0,
                            (int) (testWidthDipLandscape * dipScale),
                            (int) (testHeightDipLandscape * dipScale),
                            0,
                            0,
                            0,
                            0);
                });
        CriteriaHelper.pollUiThread(() -> !mModel.get(ILLUSTRATION_VISIBLE));

        // Dimensions resembling portrait orientation.
        final int testHeightDipPortrait = 500;
        final int testWidthDipPortrait = 320;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMediator.onLayoutChange(
                            null,
                            0,
                            0,
                            (int) (testWidthDipPortrait * dipScale),
                            (int) (testHeightDipPortrait * dipScale),
                            0,
                            0,
                            0,
                            0);
                });
        CriteriaHelper.pollUiThread(() -> mModel.get(ILLUSTRATION_VISIBLE));

        // Dimensions resembling multi-window mode.
        final int testHeightDipMultiWindow = 250;
        final int testWidthDipMultiWindow = 320;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMediator.onLayoutChange(
                            null,
                            0,
                            0,
                            (int) (testWidthDipMultiWindow * dipScale),
                            (int) (testHeightDipMultiWindow * dipScale),
                            0,
                            0,
                            0,
                            0);
                });
        CriteriaHelper.pollUiThread(() -> !mModel.get(ILLUSTRATION_VISIBLE));
    }
}
