// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.gsa.GSAContextDisplaySelection;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests system and application interaction with Contextual Search using instrumentation tests.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
// NOTE: Disable online detection so we we'll default to online on test bots with no network.
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "disable-features=" + ChromeFeatureList.CONTEXTUAL_SEARCH_THIN_WEB_VIEW_IMPLEMENTATION})
@EnableFeatures(ChromeFeatureList.CONTEXTUAL_SEARCH_DISABLE_ONLINE_DETECTION)
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@Batch(Batch.PER_CLASS)
public class ContextualSearchObserverTest extends ContextualSearchInstrumentationBase {
    @Override
    @Before
    public void setUp() throws Exception {
        mTestPage = "/chrome/test/data/android/contextualsearch/tap_test.html";
        super.setUp();
    }

    //============================================================================================
    // Calls to ContextualSearchObserver.
    //============================================================================================

    private static class TestContextualSearchObserver implements ContextualSearchObserver {
        private int mShowCount;
        private int mShowRedactedCount;
        private int mHideCount;
        private int mFirstShownLength;
        private int mLastShownLength;

        @Override
        public void onShowContextualSearch(@Nullable GSAContextDisplaySelection selectionContext) {
            mShowCount++;
            if (selectionContext != null
                    && selectionContext.startOffset < selectionContext.endOffset) {
                mLastShownLength = selectionContext.endOffset - selectionContext.startOffset;
                if (mFirstShownLength == 0) mFirstShownLength = mLastShownLength;
            } else {
                mShowRedactedCount++;
            }
        }

        @Override
        public void onHideContextualSearch() {
            mHideCount++;
        }

        /**
         * @return The count of Hide notifications sent to observers.
         */
        int getHideCount() {
            return mHideCount;
        }

        /**
         * @return The count of Show notifications sent to observers.
         */
        int getShowCount() {
            return mShowCount;
        }

        /**
         * @return The count of Show notifications sent to observers that had the data redacted due
         *         to our policy on privacy.
         */
        int getShowRedactedCount() {
            return mShowRedactedCount;
        }

        /**
         * @return The length of the selection for the first Show notification.
         */
        int getFirstShownLength() {
            return mFirstShownLength;
        }

        /**
         * @return The length of the selection for the last Show notification.
         */
        int getLastShownLength() {
            return mLastShownLength;
        }
    }

    /**
     * Tests that a ContextualSearchObserver gets notified when the user brings up The Contextual
     * Search panel via long press and then dismisses the panel by tapping on the base page.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testNotifyObserversAfterNonResolve() throws Exception {
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        TestThreadUtils.runOnUiThreadBlocking(() -> mManager.addObserver(observer));
        triggerNonResolve(SEARCH_NODE);
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(0, observer.getHideCount());

        tapBasePageToClosePanel();
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        TestThreadUtils.runOnUiThreadBlocking(() -> mManager.removeObserver(observer));
    }

    /**
     * Tests that a ContextualSearchObserver gets notified without any page context when the user
     * is Undecided and our policy disallows sending surrounding text.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    // Previously flaky and disabled 4/2021.  https://crbug.com/1180304
    public void testNotifyObserversAfterLongPressWithoutSurroundings(
            @EnabledFeature int enabledFeature) throws Exception {
        // Mark the user undecided so we won't allow sending surroundings.
        mPolicy.overrideDecidedStateForTesting(false);
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        TestThreadUtils.runOnUiThreadBlocking(() -> mManager.addObserver(observer));
        triggerNonResolve(SEARCH_NODE);
        Assert.assertEquals(1, observer.getShowRedactedCount());
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(0, observer.getHideCount());

        tapBasePageToClosePanel();
        Assert.assertEquals(1, observer.getShowRedactedCount());
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        TestThreadUtils.runOnUiThreadBlocking(() -> mManager.removeObserver(observer));
    }

    /**
     * Tests that ContextualSearchObserver gets notified when user brings up contextual search
     * panel and then dismisses the panel by tapping on the base page.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testNotifyObserversAfterResolve() throws Exception {
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        TestThreadUtils.runOnUiThreadBlocking(() -> mManager.addObserver(observer));
        simulateResolveSearch(SEARCH_NODE);
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(0, observer.getHideCount());

        tapBasePageToClosePanel();
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        TestThreadUtils.runOnUiThreadBlocking(() -> mManager.removeObserver(observer));
    }

    /**
     * Tests that ContextualSearchObserver gets notified when the user brings up the contextual
     * search panel via long press and then dismisses the panel by tapping copy (hide select action
     * mode).
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testNotifyObserversOnClearSelectionAfterLongpress() throws Exception {
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        TestThreadUtils.runOnUiThreadBlocking(() -> mManager.addObserver(observer));
        longPressNode(SEARCH_NODE);
        Assert.assertEquals(0, observer.getHideCount());

        // Dismiss select action mode.
        assertWaitForSelectActionBarVisible(true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> getSelectionPopupController().destroySelectActionMode());
        assertWaitForSelectActionBarVisible(false);

        waitForPanelToClose();
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        TestThreadUtils.runOnUiThreadBlocking(() -> mManager.removeObserver(observer));
    }

    /**
     * Tests that expanding the selection during a Search Term Resolve notifies the observers before
     * and after the expansion.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testNotifyObserversOnExpandSelection() throws Exception {
        mPolicy.overrideDecidedStateForTesting(true);
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        TestThreadUtils.runOnUiThreadBlocking(() -> mManager.addObserver(observer));

        simulateSlowResolveSearch("states");
        simulateSlowResolveFinished();
        closePanel();

        Assert.assertEquals("States".length(), observer.getFirstShownLength());
        Assert.assertEquals("United States".length(), observer.getLastShownLength());
        Assert.assertEquals(2, observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        TestThreadUtils.runOnUiThreadBlocking(() -> mManager.removeObserver(observer));
    }

    /** Asserts that the given value is either 1 or 2.  Helpful for flaky tests. */
    private void assertValueIs1or2(int value) {
        if (value != 1) Assert.assertEquals(2, value);
    }

    /**
     * Tests a second Tap: a Tap on an existing tap-selection.
     * TODO(donnd): move to the section for observer tests.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisabledTest(
            message =
                    "Flaking on multiple bots, see https://crbug.com/1403674 and https://crbug.com/1459535")
    public void
    testSecondTap() throws Exception {
        TestContextualSearchObserver observer = new TestContextualSearchObserver();
        TestThreadUtils.runOnUiThreadBlocking(() -> mManager.addObserver(observer));

        clickWordNode("search");
        Assert.assertEquals(1, observer.getShowCount());
        Assert.assertEquals(0, observer.getHideCount());

        clickNode("search");
        waitForSelectActionBarVisible();
        closePanel();

        // Sometimes we get an additional Show notification on the second Tap, but not reliably in
        // tests.  See crbug.com/776541.
        assertValueIs1or2(observer.getShowCount());
        Assert.assertEquals(1, observer.getHideCount());
        TestThreadUtils.runOnUiThreadBlocking(() -> mManager.removeObserver(observer));
    }
}
