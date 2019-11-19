// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import static org.hamcrest.Matchers.lessThan;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import android.support.test.filters.SmallTest;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.browser.widget.ScrimView.ScrimObserver;
import org.chromium.chrome.browser.widget.ScrimView.ScrimParams;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.TestBottomSheetContent;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.concurrent.TimeoutException;

/**
 * This class tests the behavior of the scrim with the various components that interact with it. The
 * two primary uses are letting the scrim animate manually (as it is used with with omnibox) and
 * manually changing the scrim's alpha (as the bottom sheet uses it).
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class ScrimTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private BottomSheetController mSheetController;
    private ScrimView mScrim;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        final ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetController = activity.getBottomSheetController();
            mScrim = activity.getScrim();
        });
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testScrimVisibility() throws TimeoutException {
        CallbackHelper visibilityHelper = new CallbackHelper();
        ScrimObserver observer = new ScrimObserver() {
            @Override
            public void onScrimClick() {}

            @Override
            public void onScrimVisibilityChanged(boolean visible) {
                visibilityHelper.notifyCalled();
            }
        };

        final ScrimParams params =
                new ScrimParams(mActivityTestRule.getActivity().getCompositorViewHolder(), true,
                        false, 0, observer);

        int callCount = visibilityHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mScrim.showScrim(params);
            // Skip the animation and set the scrim opacity to 50%.
            mScrim.setViewAlpha(0.5f);
        });
        visibilityHelper.waitForCallback(callCount, 1);
        assertScrimVisibility(true);

        callCount = visibilityHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> mScrim.hideScrim(false));
        visibilityHelper.waitForCallback(callCount, 1);
        assertScrimVisibility(false);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testBottomSheetScrim() {
        mScrim.disableAnimationForTesting(true);
        assertScrimVisibility(false);
        assertFalse("Nothing should be obscuring the tab.",
                mActivityTestRule.getActivity().isViewObscuringAllTabs());
        assertEquals("The scrim alpha should be 0.", 0f, mScrim.getAlpha(), MathUtils.EPSILON);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetController.requestShowContent(
                    new TestBottomSheetContent(mActivityTestRule.getActivity(),
                            BottomSheetContent.ContentPriority.HIGH, false),
                    false);
            mSheetController.setSheetStateForTesting(BottomSheetController.SheetState.HALF, false);
        });

        assertScrimVisibility(true);
        assertTrue("A view should be obscuring the tab.",
                mActivityTestRule.getActivity().isViewObscuringAllTabs());
        assertEquals("The scrim alpha should be 0.", 0f, mScrim.getAlpha(), MathUtils.EPSILON);

        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mSheetController.setSheetStateForTesting(
                                BottomSheetController.SheetState.PEEK, false));

        assertScrimVisibility(false);
        assertFalse("Nothing should be obscuring the tab.",
                mActivityTestRule.getActivity().isViewObscuringAllTabs());
        assertEquals("The scrim alpha should be 0.", 0f, mScrim.getAlpha(), MathUtils.EPSILON);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    @DisabledTest(message = "crbug.com/877774")
    public void testOmniboxScrim() {
        assertScrimVisibility(false);
        assertFalse("Nothing should be obscuring the tab.",
                mActivityTestRule.getActivity().isViewObscuringAllTabs());
        assertEquals("The scrim alpha should be 0.", 0f, mScrim.getAlpha(), MathUtils.EPSILON);

        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        ThreadUtils.runOnUiThreadBlocking(() -> OmniboxTestUtils.toggleUrlBarFocus(urlBar, true));
        waitForScrimVisibilityChange(true);

        assertScrimVisibility(true);
        assertTrue("A view should be obscuring the tab.",
                mActivityTestRule.getActivity().isViewObscuringAllTabs());
        assertThat("The scrim alpha should not be 0.", 0f, lessThan(mScrim.getAlpha()));

        ThreadUtils.runOnUiThreadBlocking(() -> OmniboxTestUtils.toggleUrlBarFocus(urlBar, false));
        waitForScrimVisibilityChange(false);

        assertScrimVisibility(false);
        assertFalse("Nothing should be obscuring the tab.",
                mActivityTestRule.getActivity().isViewObscuringAllTabs());
        assertEquals("The scrim alpha should be 0.", 0f, mScrim.getAlpha(), MathUtils.EPSILON);
    }

    /**
     * Assert that the scrim is the desired visibility.
     * @param visible Whether the scrim should be visible.
     */
    private void assertScrimVisibility(final boolean visible) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            if (visible) {
                assertEquals("The scrim should be visible.", View.VISIBLE, mScrim.getVisibility());
            } else {
                assertEquals("The scrim should be invisible.", View.GONE, mScrim.getVisibility());
            }
        });
    }

    /**
     * Wait for the visibility of the scrim to change.
     * @param visible Whether the scrim should be visible.
     */
    private void waitForScrimVisibilityChange(boolean visible) {
        CriteriaHelper.pollUiThread(() -> {
            return (!visible && mScrim.getVisibility() != View.VISIBLE)
                    || (visible && mScrim.getVisibility() == View.VISIBLE);
        });
    }
}
