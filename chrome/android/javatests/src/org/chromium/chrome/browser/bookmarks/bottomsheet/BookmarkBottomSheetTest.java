// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bottomsheet;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Test to verify bookmark bottom sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BookmarkBottomSheetTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private BookmarkBottomSheetCoordinator mBottomSheetCoordinator;
    private BottomSheetController mBottomSheetController;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mBottomSheetController = mActivityTestRule.getActivity()
                                         .getRootUiCoordinatorForTesting()
                                         .getBottomSheetController();
        mBottomSheetCoordinator = new BookmarkBottomSheetCoordinator(
                mActivityTestRule.getActivity(), mBottomSheetController);
    }

    private void showBottomSheet() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mBottomSheetCoordinator.show(); });

        CriteriaHelper.pollUiThread(
                () -> mBottomSheetController.getSheetState() == SheetState.FULL);
    }

    @Test
    @MediumTest
    public void testBottomSheetShow() {
        showBottomSheet();
        onView(withId(R.id.sheet_title)).check(matches(isDisplayed()));
    }
}
