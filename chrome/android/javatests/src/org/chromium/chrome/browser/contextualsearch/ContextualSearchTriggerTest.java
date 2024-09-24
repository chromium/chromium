// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.text.TextUtils;
import android.view.ViewConfiguration;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests the Related Searches Feature of Contextual Search using instrumentation tests. */
@RunWith(ChromeJUnit4ClassRunner.class)
// NOTE: Disable online detection so we we'll default to online on test bots with no network.
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.CONTEXTUAL_SEARCH_DISABLE_ONLINE_DETECTION)
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@Batch(Batch.PER_CLASS)
public class ContextualSearchTriggerTest extends ContextualSearchInstrumentationBase {
    @Override
    @Before
    public void setUp() throws Exception {
        mTestPage = "/chrome/test/data/android/contextualsearch/tap_test.html";
        super.setUp();
    }

    // ============================================================================================
    // Test Cases
    // ============================================================================================

    /** Tests the doesContainAWord method. TODO(donnd): Change to a unit test. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testDoesContainAWord() {
        Assert.assertTrue(mSelectionController.doesContainAWord("word"));
        Assert.assertTrue(mSelectionController.doesContainAWord("word "));
        Assert.assertFalse(
                "Emtpy string should not be considered a word!",
                mSelectionController.doesContainAWord(""));
        Assert.assertFalse(
                "Special symbols should not be considered a word!",
                mSelectionController.doesContainAWord("@"));
        Assert.assertFalse(
                "White space should not be considered a word",
                mSelectionController.doesContainAWord(" "));
        Assert.assertTrue(mSelectionController.doesContainAWord("Q2"));
        Assert.assertTrue(mSelectionController.doesContainAWord("123"));
    }

    /** Tests the isValidSelection method. TODO(donnd): Change to a unit test. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testIsValidSelection() {
        StubbedSelectionPopupController c = new StubbedSelectionPopupController();
        Assert.assertTrue(mSelectionController.isValidSelection("valid", c));
        Assert.assertFalse(mSelectionController.isValidSelection(" ", c));
        c.setIsFocusedNodeEditableForTest(true);
        Assert.assertFalse(mSelectionController.isValidSelection("editable", c));
        c.setIsFocusedNodeEditableForTest(false);
        String numberString = "0123456789";
        Assert.assertTrue(mSelectionController.isValidSelection(numberString, c));
        StringBuilder longStringBuilder = new StringBuilder().append(numberString);
        for (int i = 0; i < 10; i++) {
            longStringBuilder.append(longStringBuilder.toString());
            if (longStringBuilder.toString().length() < 1000) {
                Assert.assertTrue(
                        mSelectionController.isValidSelection(longStringBuilder.toString(), c));
            } else {
                Assert.assertFalse(
                        mSelectionController.isValidSelection(longStringBuilder.toString(), c));
                break;
            }
        }
    }

    /** Tests a simple non-resolving gesture, without opening the panel. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testNonResolveTrigger() throws Exception {
        triggerNonResolve("states");

        Assert.assertNull(mFakeServer.getSearchTermRequested());
        waitForPanelToPeek();
        assertLoadedNoUrl();
        assertNoWebContents();
    }

    // ============================================================================================
    // Tap=gesture Tests
    // ============================================================================================

    /** Tests that a Tap gesture on a special character does not select or show the panel. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously flaky and disabled 4/2021.  https://crbug.com/1180304
    public void testTapGestureOnSpecialCharacterDoesntSelect() throws Exception {
        clickNode("question-mark");
        Assert.assertNull(getSelectedText());
        assertPanelClosedOrUndefined();
        assertLoadedNoUrl();
    }

    /** Tests that a Tap gesture followed by scrolling clears the selection. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapGestureFollowedByScrollClearsSelection() throws Exception {
        clickWordNode("intelligence");
        fakeResponse(false, 200, "Intelligence", "Intelligence", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeek();
        assertLoadedLowPriorityUrl();
        scrollBasePage();
        assertPanelClosedOrUndefined();
        Assert.assertTrue(TextUtils.isEmpty(mSelectionController.getSelectedText()));
    }

    /** Tests that a Tap gesture followed by tapping an invalid character doesn't select. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously flaky and disabled 4/2021.  https://crbug.com/1192285
    public void testTapGestureFollowedByInvalidTextTapCloses() throws Exception {
        clickWordNode("states-far");
        waitForPanelToPeek();
        clickNode("question-mark");
        waitForPanelToClose();
        Assert.assertNull(mSelectionController.getSelectedText());
    }

    /** Tests that a Tap gesture followed by tapping a non-text element doesn't select. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisabledTest(message = "crbug.com/662104")
    public void testTapGestureFollowedByNonTextTap() throws Exception {
        clickWordNode("states-far");
        waitForPanelToPeek();
        clickNode("button");
        waitForPanelToCloseAndSelectionEmpty();
    }

    /** Tests that a Tap gesture far away toggles selecting text. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapGestureFarAwayTogglesSelecting() throws Exception {
        clickWordNode("states");
        Assert.assertEquals("States", getSelectedText());
        waitForPanelToPeek();
        clickNode("states-far");
        waitForPanelToClose();
        Assert.assertNull(getSelectedText());
        clickNode("states-far");
        waitForPanelToPeek();
        Assert.assertEquals("States", getSelectedText());
    }

    /** Tests a "tap-near" -- that sequential Tap gestures nearby keep selecting. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously disabled at https://crbug.com/1075895
    @DisabledTest(message = "See crbug.com/1455161") // Disabled because it is flaky
    public void testTapGesturesNearbyKeepSelecting() throws Exception {
        clickWordNode("states");
        Assert.assertEquals("States", getSelectedText());
        waitForPanelToPeek();
        // Avoid issues with double-tap detection by ensuring sequential taps
        // aren't treated as such. Double-tapping can also select words much as
        // longpress, in turn showing the pins and preventing contextual tap
        // refinement from nearby taps. The double-tap timeout is sufficiently
        // short that this shouldn't conflict with tap refinement by the user.
        Thread.sleep(ViewConfiguration.getDoubleTapTimeout());
        // Because sequential taps never hide the bar, we we can't wait for it to peek.
        // Instead we use clickNode (which doesn't wait) instead of clickWordNode and wait
        // for the selection to change.
        clickNode("states-near");
        waitForSelectionToBe("StatesNear");
        Thread.sleep(ViewConfiguration.getDoubleTapTimeout());
        clickNode("states");
        waitForSelectionToBe("States");
    }

    // ============================================================================================
    // Long-press non-triggering gesture tests.
    // ============================================================================================

    /** Tests that a long-press gesture followed by scrolling does not clear the selection. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testLongPressGestureFollowedByScrollMaintainsSelection() throws Exception {
        longPressNode("intelligence");
        waitForPanelToPeek();
        scrollBasePage();
        assertPanelClosedOrUndefined();
        Assert.assertEquals("Intelligence", getSelectedText());
        assertLoadedNoUrl();
    }

    /** Tests that a long-press gesture followed by a tap does not select. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(DeviceFormFactor.PHONE)
    @DisabledTest(message = "See https://crbug.com/837998")
    public void testLongPressGestureFollowedByTapDoesntSelect() throws Exception {
        longPressNode("intelligence");
        waitForPanelToPeek();
        clickWordNode("states-far");
        waitForGestureToClosePanelAndAssertNoSelection();
        assertLoadedNoUrl();
    }

    /** Tests suppression of any triggering on small view heights. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @EnableFeatures(ChromeFeatureList.CONTEXTUAL_SEARCH_SUPPRESS_SHORT_VIEW)
    public void testIsSuppressedOnViewHeight_ridiculouslyShort() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.CONTEXTUAL_SEARCH_SUPPRESS_SHORT_VIEW,
                ContextualSearchFieldTrial.CONTEXTUAL_SEARCH_MINIMUM_PAGE_HEIGHT_NAME,
                "100");
        FeatureList.setTestValues(testValues);
        Assert.assertFalse(mContextualSearchManager.isSuppressed());
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @EnableFeatures(ChromeFeatureList.CONTEXTUAL_SEARCH_SUPPRESS_SHORT_VIEW)
    public void testIsSuppressedOnViewHeight_ridiculouslyTall() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.CONTEXTUAL_SEARCH_SUPPRESS_SHORT_VIEW,
                ContextualSearchFieldTrial.CONTEXTUAL_SEARCH_MINIMUM_PAGE_HEIGHT_NAME,
                "500000");
        FeatureList.setTestValues(testValues);
        Assert.assertTrue(mContextualSearchManager.isSuppressed());
    }

    // ============================================================================================
    // Tap-non-triggering when ARIA annotated as interactive.
    // ============================================================================================

    /** Tests that a Tap gesture on an element with an ARIA role does not trigger. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapOnRoleIgnored() throws Exception {
        @OverlayPanel.PanelState int initialState = mPanel.getPanelState();
        clickNode("role");
        assertPanelStillInState(initialState);
    }

    /**
     * Tests that a Tap gesture on an element with an ARIA attribute does not trigger.
     * http://crbug.com/542874
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapOnARIAIgnored() throws Exception {
        @OverlayPanel.PanelState int initialState = mPanel.getPanelState();
        clickNode("aria");
        assertPanelStillInState(initialState);
    }

    /** Tests that a Tap gesture on an element that is focusable does not trigger. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapOnFocusableIgnored() throws Exception {
        @OverlayPanel.PanelState int initialState = mPanel.getPanelState();
        clickNode("focusable");
        assertPanelStillInState(initialState);
    }

    // ============================================================================================
    // Search-term resolution (server request to determine a search).
    // ============================================================================================

    /**
     * Tests expanding the panel before the search term has resolved, verifies that nothing loads
     * until the resolve completes and that it's now a normal priority URL.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testExpandBeforeSearchTermResolution() throws Exception {
        simulateSlowResolveSearch("states");
        assertNoWebContents();

        // Expanding before the search term resolves should not load anything.
        expandPanelAndAssert();
        assertLoadedNoUrl();

        // Once the response comes in, it should load.
        simulateSlowResolveFinished();
        assertContainsParameters("States");
        assertLoadedNormalPriorityUrl();
        assertWebContentsCreated();
        assertWebContentsVisible();
    }

    /**
     * Tests that the Contextual Search panel does not reappear when a long-press selection is
     * modified after the user has taken an action to explicitly dismiss the panel. Also tests that
     * the panel reappears when a new selection is made.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously flaky, disabled 4/2021.  https://crbug.com/1192285, https://crbug.com/1291558
    public void testPreventHandlingCurrentSelectionModification() throws Exception {
        longPressNode("search");

        // Dismiss the Contextual Search panel.
        closePanel();
        Assert.assertEquals("Search", getSelectedText());

        // Simulate a selection change event and assert that the panel has not reappeared.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SelectionClient selectionClient = mManager.getContextualSearchSelectionClient();
                    selectionClient.onSelectionEvent(
                            org.chromium.ui.touch_selection.SelectionEventType
                                    .SELECTION_HANDLE_DRAG_STARTED,
                            333,
                            450);
                    selectionClient.onSelectionEvent(
                            org.chromium.ui.touch_selection.SelectionEventType
                                    .SELECTION_HANDLE_DRAG_STOPPED,
                            303,
                            450);
                });
        assertPanelClosedOrUndefined();

        // Select a different word and assert that the panel has appeared.
        longPressNode("resolution");
        // The simulateNonResolveSearch call will verify that the panel peeks.
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously flaky and disabled 4/2021.  https://crbug.com/1180304
    public void testSelectionExpansionOnSearchTermResolution() throws Exception {
        triggerResolve("intelligence");
        waitForPanelToPeek();

        ResolvedSearchTerm resolvedSearchTerm =
                new ResolvedSearchTerm.Builder(
                                false, 200, "Intelligence", "United States Intelligence")
                        .setSelectionStartAdjust(-14)
                        .build();
        fakeResponse(resolvedSearchTerm);
        waitForSelectionToBe("United States Intelligence");
    }

    // ============================================================================================
    // Read Aloud Tap to Seek Suppression
    // ============================================================================================

    /**
     * Tests that Contextual Search does not show when ReadAloud has an active playback on the tab.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch", "ReadAloud"})
    @EnableFeatures(ChromeFeatureList.READALOUD_TAP_TO_SEEK)
    public void testTapToSeekSuppression() throws Exception {
        changeReadAloudActivePlaybackTab();

        clickNode("intelligence");
        Assert.assertEquals(null, getSelectedText());
    }
}
