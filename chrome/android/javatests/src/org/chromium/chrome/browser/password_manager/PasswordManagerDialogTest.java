// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.Espresso.pressBack;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.ILLUSTRATION_VISIBLE;

import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/** Test for the password manager illustration modal dialog. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordManagerDialogTest {
    private PasswordManagerDialogCoordinator mCoordinator;
    private PasswordManagerDialogMediator mMediator;
    private PropertyModel mModel;
    private static final String TITLE = "Title";
    private static final String DETAILS = "Explanation text.";
    private static final String OK_BUTTON = "OK";
    private static final String CANCEL_BUTTON = "Cancel";

    @Mock
    private Callback<Integer> mOnClick;

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        ChromeActivity activity = (ChromeActivity) mActivityTestRule.getActivity();
        mCoordinator = new PasswordManagerDialogCoordinator(
                activity.getWindowAndroid().getContext().get(), activity.getModalDialogManager(),
                activity.findViewById(android.R.id.content), activity.getFullscreenManager(),
                activity.getControlContainerHeightResource());
        mMediator = mCoordinator.getMediatorForTesting();
        mModel = mMediator.getModelForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCoordinator.showDialog(TITLE, DETAILS, R.drawable.data_reduction_illustration,
                    OK_BUTTON, CANCEL_BUTTON, mOnClick, false,
                    ModalDialogManager.ModalDialogType.TAB);
        });
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
        verify(mOnClick).onResult(DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
    }

    @Test
    @SmallTest
    public void testSettingImageVisibility() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mModel.set(ILLUSTRATION_VISIBLE, false); });
        onView(withId(R.id.password_manager_dialog_illustration))
                .check(matches(not(isDisplayed())));
        TestThreadUtils.runOnUiThreadBlocking(() -> { mModel.set(ILLUSTRATION_VISIBLE, true); });
        onView(withId(R.id.password_manager_dialog_illustration)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testWatchingLayoutChanges() {
        float dipScale =
                mActivityTestRule.getActivity().getWindowAndroid().getDisplay().getDipScale();

        // Dimensions resembling landscape orientation.
        final int testHeightDipLandscape = 300; // Height of the android content view.
        final int testWidthDipLandscape = 500; // Width of the android content view.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMediator.onLayoutChange(null, 0, 0, (int) (testWidthDipLandscape * dipScale),
                    (int) (testHeightDipLandscape * dipScale), 0, 0, 0, 0);
        });
        CriteriaHelper.pollUiThread(() -> !mModel.get(ILLUSTRATION_VISIBLE));

        // Dimensions resembling portrait orientation.
        final int testHeightDipPortrait = 500;
        final int testWidthDipPortrait = 320;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMediator.onLayoutChange(null, 0, 0, (int) (testWidthDipPortrait * dipScale),
                    (int) (testHeightDipPortrait * dipScale), 0, 0, 0, 0);
        });
        CriteriaHelper.pollUiThread(() -> mModel.get(ILLUSTRATION_VISIBLE));

        // Dimensions resembling multi-window mode.
        final int testHeightDipMultiWindow = 250;
        final int testWidthDipMultiWindow = 320;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMediator.onLayoutChange(null, 0, 0, (int) (testWidthDipMultiWindow * dipScale),
                    (int) (testHeightDipMultiWindow * dipScale), 0, 0, 0, 0);
        });
        CriteriaHelper.pollUiThread(() -> !mModel.get(ILLUSTRATION_VISIBLE));
    }
}
