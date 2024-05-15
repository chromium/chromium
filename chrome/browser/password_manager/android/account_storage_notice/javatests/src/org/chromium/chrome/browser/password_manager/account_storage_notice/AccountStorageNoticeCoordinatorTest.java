// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.account_storage_notice;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Tests for AccountStorageNoticeCoordinator. */
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@RunWith(ChromeJUnit4ClassRunner.class)
public class AccountStorageNoticeCoordinatorTest {
    @Rule public ChromeTabbedActivityTestRule mActivityRule = new ChromeTabbedActivityTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private AccountStorageNoticeCoordinator.Natives mJniMock;

    private static final long NATIVE_OBSERVER_PTR = 42;
    // IDS_PASSWORDS_ACCOUNT_STORAGE_NOTICE_SUBTITLE without the <link> tags. It seems best for the
    // test to be explicit rather than implement string manipulation.
    private static final String RAW_SUBTITLE_TEXT =
            "When you’re signed in to Chrome, passwords you save will go in your Google Account. To"
                    + " turn this off, go to settings.";

    @Before
    public void setUp() {
        mJniMocker.mock(AccountStorageNoticeCoordinatorJni.TEST_HOOKS, mJniMock);
        mActivityRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    public void testStrings() {
        createCoordinator();
        waitSheetVisible(true);

        onView(withText(R.string.passwords_account_storage_notice_title))
                .check(matches(isDisplayed()));
        onView(withText(R.string.passwords_account_storage_notice_button_text))
                .check(matches(isDisplayed()));
        onView(withText(RAW_SUBTITLE_TEXT)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testClickButton() {
        createCoordinator();
        waitSheetVisible(true);
        verify(mJniMock, never()).onClosed(NATIVE_OBSERVER_PTR);

        onView(withText(R.string.passwords_account_storage_notice_button_text)).perform(click());

        waitSheetVisible(false);
        verify(mJniMock).onClosed(NATIVE_OBSERVER_PTR);
    }

    // TODO(crbug.com/338576301): Add test clicking on settings link. There seems to be some
    // limitation on ViewUtils.clickOnClickableSpan().

    @Test
    @MediumTest
    public void testDismissWithBackPress() {
        createCoordinator();
        waitSheetVisible(true);
        verify(mJniMock, never()).onClosed(NATIVE_OBSERVER_PTR);

        Espresso.pressBack();

        waitSheetVisible(false);
        verify(mJniMock).onClosed(NATIVE_OBSERVER_PTR);
    }

    @Test
    @MediumTest
    public void testDestroyWhileShowing() {
        AccountStorageNoticeCoordinator coordinator = createCoordinator();
        waitSheetVisible(true);
        verify(mJniMock, never()).onClosed(NATIVE_OBSERVER_PTR);

        TestThreadUtils.runOnUiThreadBlocking(() -> coordinator.destroy());

        waitSheetVisible(false);
        verify(mJniMock, never()).onClosed(NATIVE_OBSERVER_PTR);
    }

    private AccountStorageNoticeCoordinator createCoordinator() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> {
                    return new AccountStorageNoticeCoordinator(
                            mActivityRule.getActivity().getWindowAndroid(),
                            new SettingsLauncherImpl(),
                            NATIVE_OBSERVER_PTR);
                });
    }

    private void waitSheetVisible(boolean visible) {
        CriteriaHelper.pollUiThread(
                () -> {
                    @SheetState
                    int state =
                            BottomSheetControllerProvider.from(
                                            mActivityRule.getActivity().getWindowAndroid())
                                    .getSheetState();
                    // The sheet opens at half height or full height depending on the screen size.
                    return visible
                            ? (state == SheetState.HALF || state == SheetState.FULL)
                            : state == SheetState.HIDDEN;
                });
    }
}
