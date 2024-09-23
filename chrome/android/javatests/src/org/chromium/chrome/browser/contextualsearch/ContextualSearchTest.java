// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/** Mock touch events with Contextual Search to test behavior of its panel and manager. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ContextualSearchTest extends ContextualSearchInstrumentationBase {
    @Rule public JniMocker mocker = new JniMocker();

    @Mock ContextualSearchManager.Natives mContextualSearchManagerJniMock;

    @Override
    @Before
    public void setUp() throws Exception {
        super.setUp();

        MockitoAnnotations.initMocks(this);
        mocker.mock(ContextualSearchManagerJni.TEST_HOOKS, mContextualSearchManagerJniMock);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeActivity activity = (ChromeActivity) mManager.getActivity();
                    mPanel =
                            new ContextualSearchPanelWrapper(
                                    activity,
                                    activity.getCompositorViewHolderForTesting().getLayoutManager(),
                                    mPanelManager,
                                    ProfileProvider.getOrCreateProfile(
                                            activity.getProfileProviderSupplier().get(), false));
                    mPanel.setManagementDelegate(mContextualSearchManager);
                    mContextualSearchManager.setContextualSearchPanel(mPanel);
                    mPanelManager.setDynamicResourceLoader(new DynamicResourceLoader(0, null));
                });
    }

    /** Tests that a Long-press gesture followed by tapping empty space closes the panel. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testLongpressFollowedByNonTextTap() {
        Assert.assertEquals(0, mPanelManager.getRequestPanelShowCount());

        // Fake a selection event.
        mockLongpressText("text");
        // Generate the surrounding-text-available callback.
        // Surrounding text is gathered for longpress due to icing integration.
        generateTextSurroundingSelectionAvailable();

        Assert.assertEquals(1, mPanelManager.getRequestPanelShowCount());
        Assert.assertEquals(0, mPanelManager.getPanelHideCount());
        Assert.assertEquals(
                mContextualSearchManager.getSelectionController().getSelectedText(), "text");

        // Fake tap on non-text.
        mockTapEmptySpace();

        Assert.assertEquals(1, mPanelManager.getRequestPanelShowCount());
        Assert.assertEquals(1, mPanelManager.getPanelHideCount());
        Assert.assertNull(mContextualSearchManager.getSelectionController().getSelectedText());
    }

    /** Tests that a Tap gesture followed by tapping empty space closes the panel. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTextTapFollowedByNonTextTap() {
        Assert.assertEquals(0, mPanelManager.getRequestPanelShowCount());

        // Fake a Tap event.
        mockTapText("text");
        // Generate the surrounding-text-available callback.
        generateTextSurroundingSelectionAvailable();
        // Right now the tap-processing sequence will stall at selectWordAroundCaret, so we need
        // to prod it forward by generating an ACK:
        generateSelectWordAroundCaretAck();
        Assert.assertEquals(1, mPanelManager.getRequestPanelShowCount());
        Assert.assertEquals(0, mPanelManager.getPanelHideCount());
    }

    /**
     * Tests that a Tap gesture processing is robust even when the selection somehow gets cleared
     * during that process. This tests a failure-case found in crbug.com/728644.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTapProcessIsRobustWhenSelectionGetsCleared() {
        Assert.assertEquals(0, mPanelManager.getRequestPanelShowCount());

        // Fake a Tap event.
        mockTapText("text");
        // Generate the surrounding-text-available callback.
        generateTextSurroundingSelectionAvailable();

        // Now clear the selection!
        mContextualSearchManager.getSelectionController().clearSelection();

        // Continue processing the Tap by acknowledging the SelectWordAroundCaret has selected the
        // word.  However we just simulated a condition that clears the selection above, so we're
        // testing for robustness in completion of the processing even when there's no selection.
        generateSelectWordAroundCaretAck();
        Assert.assertEquals(0, mPanelManager.getRequestPanelShowCount());
        Assert.assertEquals(0, mPanelManager.getPanelHideCount());
    }
}
