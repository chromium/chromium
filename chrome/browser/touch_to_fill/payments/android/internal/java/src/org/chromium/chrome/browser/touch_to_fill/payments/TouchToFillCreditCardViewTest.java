// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.not;
import static org.mockito.Mockito.verify;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

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
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;

/** Tests for {@link TouchToFillCreditCardView} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TouchToFillCreditCardViewTest {
    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mSheetSupport;
    private TouchToFillCreditCardCoordinator mCoordinator;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    private TouchToFillCreditCardComponent.Delegate mDelegateMock;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setupTest() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mBottomSheetController = mActivityTestRule.getActivity()
                                         .getRootUiCoordinatorForTesting()
                                         .getBottomSheetController();
        mSheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(() -> {
            mCoordinator = new TouchToFillCreditCardCoordinator();
            mCoordinator.initialize(
                    mActivityTestRule.getActivity(), mBottomSheetController, mDelegateMock);
        });
    }

    @Test
    @MediumTest
    public void testScanNewCardButtonIsHidden() {
        runOnUiThreadBlocking(() -> mCoordinator.showSheet(false));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        runOnUiThreadBlocking(() -> { mSheetSupport.setSheetState(SheetState.FULL, false); });

        onView(withId(R.id.scan_new_card)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testScanNewCardClick() {
        runOnUiThreadBlocking(() -> mCoordinator.showSheet(true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        runOnUiThreadBlocking(() -> { mSheetSupport.setSheetState(SheetState.FULL, false); });

        onView(withId(R.id.scan_new_card)).perform(click());

        verify(mDelegateMock).scanCreditCard();
    }
}
