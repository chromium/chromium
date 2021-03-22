// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.content.Context;
import android.widget.LinearLayout;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManagerWrapper;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestSelectionPopupController;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.touch_selection.SelectionEventType;

/**
 * Mock touch events with Contextual Search to test behavior of its panel and manager.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(donnd): Add parameterized testing so Long-press resolve and Translations
// can be tested too.  Or just remove this whole suite if it's not useful for
// these experimental triggering changes.
@Features.DisableFeatures({ChromeFeatureList.CONTEXTUAL_SEARCH_LONGPRESS_RESOLVE,
        ChromeFeatureList.CONTEXTUAL_SEARCH_TRANSLATIONS,
        ChromeFeatureList.CONTEXTUAL_SEARCH_LITERAL_SEARCH_TAP})
@Batch(Batch.PER_CLASS)
public class ContextualSearchTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    ContextualSearchManager.Natives mContextualSearchManagerJniMock;

    private ContextualSearchManagerWrapper mContextualSearchManager;
    private ContextualSearchPanel mPanel;
    private OverlayPanelManagerWrapper mPanelManager;
    private SelectionClient mContextualSearchClient;
    private LayoutManagerImpl mLayoutManager;

    // --------------------------------------------------------------------------------------------

    /**
     * ContextualSearchPanel wrapper that prevents native calls.
     */
    private static class ContextualSearchPanelWrapper extends ContextualSearchPanel {
        public ContextualSearchPanelWrapper(Context context, LayoutManagerImpl layoutManager,
                OverlayPanelManager panelManager) {
            super(context, layoutManager, panelManager);
        }

        @Override
        public void peekPanel(@StateChangeReason int reason) {
            setHeightForTesting(1);
            super.peekPanel(reason);
        }

        @Override
        public void setBasePageTextControlsVisibility(boolean visible) {}
    }

    // --------------------------------------------------------------------------------------------

    /**
     * ContextualSearchManager wrapper that prevents network requests and most native calls.
     */
    private static class ContextualSearchManagerWrapper extends ContextualSearchManager {
        public ContextualSearchManagerWrapper(ChromeActivity activity) {
            super(activity, null, activity.getRootUiCoordinatorForTesting().getScrimCoordinator(),
                    activity.getActivityTabProvider(), activity.getFullscreenManager(),
                    activity.getBrowserControlsManager(), activity.getWindowAndroid(),
                    activity.getTabModelSelector(), () -> activity.getLastUserInteractionTime());
            setSelectionController(new MockCSSelectionController(activity, this));
            WebContents webContents = WebContentsFactory.createWebContents(
                    Profile.getLastUsedRegularProfile(), false);
            ContentView cv = ContentView.createContentView(
                    activity, null /* eventOffsetHandler */, webContents);
            webContents.initialize(null, ViewAndroidDelegate.createBasicDelegate(cv), null,
                    activity.getWindowAndroid(), WebContents.createDefaultInternalsHolder());
            SelectionPopupController selectionPopupController =
                    WebContentsUtils.createSelectionPopupController(webContents);
            selectionPopupController.setSelectionClient(this.getContextualSearchSelectionClient());
            MockContextualSearchPolicy policy =
                    new MockContextualSearchPolicy(getSelectionController());
            setContextualSearchPolicy(policy);
            getSelectionController().setPolicy(policy);
        }

        @Override
        public void startSearchTermResolutionRequest(
                String selection, boolean isExactResolve, ContextualSearchContext searchContext) {
            // Skip native calls and immediately "resolve" the search term.
            onSearchTermResolutionResponse(true, 200, selection, selection, "", "", false, 0, 10,
                    "", "", "", "", QuickActionCategory.NONE, 0, "", "", 0, new String[0]);
        }

        /**
         * @return A stubbed SelectionPopupController for mocking text selection.
         */
        public StubbedSelectionPopupController getBaseSelectionPopupController() {
            return (StubbedSelectionPopupController) getSelectionController()
                    .getSelectionPopupController();
        }
    }

    // --------------------------------------------------------------------------------------------

    /**
     * Selection controller that mocks out anything to do with a WebContents.
     */
    private static class MockCSSelectionController extends ContextualSearchSelectionController {
        private StubbedSelectionPopupController mPopupController;

        public MockCSSelectionController(
                ChromeActivity activity, ContextualSearchSelectionHandler handler) {
            super(activity, handler, activity.getActivityTabProvider(),
                    activity.getBrowserControlsManager());
            mPopupController = new StubbedSelectionPopupController();
        }

        @Override
        protected SelectionPopupController getSelectionPopupController() {
            return mPopupController;
        }
    }

    // --------------------------------------------------------------------------------------------

    /**
     * A SelectionPopupController that has some methods stubbed out for testing.
     */
    private static final class StubbedSelectionPopupController
            extends TestSelectionPopupController {
        private String mCurrentText;

        public StubbedSelectionPopupController() {}

        @Override
        public String getSelectedText() {
            return mCurrentText;
        }

        public void setSelectedText(String string) {
            mCurrentText = string;
        }
    }

    // --------------------------------------------------------------------------------------------

    /**
     * Trigger text selection on the contextual search manager.
     */
    private void mockLongpressText(String text) {
        mContextualSearchManager.getBaseSelectionPopupController().setSelectedText(text);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mContextualSearchClient.onSelectionEvent(
                                SelectionEventType.SELECTION_HANDLES_SHOWN, 0, 0));
    }

    /**
     * Trigger text selection on the contextual search manager.
     */
    private void mockTapText(String text) {
        mContextualSearchManager.getBaseSelectionPopupController().setSelectedText(text);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContextualSearchManager.getGestureStateListener().onTouchDown();
            mContextualSearchManager.onShowUnhandledTapUIIfNeeded(0, 0, 12, 100);
        });
    }

    /**
     * Trigger empty space tap.
     */
    private void mockTapEmptySpace() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContextualSearchManager.onShowUnhandledTapUIIfNeeded(0, 0, 0, 0);
            mContextualSearchClient.onSelectionEvent(
                    SelectionEventType.SELECTION_HANDLES_CLEARED, 0, 0);
        });
    }

    /**
     * Generates a call indicating that surrounding text and selection range are available.
     */
    private void generateTextSurroundingSelectionAvailable() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // It only makes sense to send dummy data here because we can't easily control
            // what's in the native context.
            mContextualSearchManager.onTextSurroundingSelectionAvailable("UTF-8", "unused", 0, 0);
        });
    }

    /**
     * Generates an ACK for the SelectWordAroundCaret native call, which indicates that the select
     * action has completed with the given result.
     */
    private void generateSelectWordAroundCaretAck() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // It only makes sense to send dummy data here because we can't easily control
            // what's in the native context.
            mContextualSearchClient.selectWordAroundCaretAck(true, 0, 0);
        });
    }

    // --------------------------------------------------------------------------------------------

    @Before
    public void setUp() throws Exception {
        final ChromeActivity activity = sActivityTestRule.getActivity();
        MockitoAnnotations.initMocks(this);
        mocker.mock(ContextualSearchManagerJni.TEST_HOOKS, mContextualSearchManagerJniMock);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPanelManager = new OverlayPanelManagerWrapper();
            mPanelManager.setContainerView(new LinearLayout(activity));
            mPanelManager.setDynamicResourceLoader(new DynamicResourceLoader(0, null));

            mContextualSearchManager = new ContextualSearchManagerWrapper(activity);
            mPanel = new ContextualSearchPanelWrapper(
                    activity, activity.getCompositorViewHolder().getLayoutManager(), mPanelManager);
            mPanel.setManagementDelegate(mContextualSearchManager);
            mContextualSearchManager.setContextualSearchPanel(mPanel);

            mContextualSearchClient = mContextualSearchManager.getContextualSearchSelectionClient();
        });
    }

    /**
     * Tests that a Long-press gesture followed by tapping empty space closes the panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testLongpressFollowedByNonTextTap() {
        Assert.assertEquals(mPanelManager.getRequestPanelShowCount(), 0);

        // Fake a selection event.
        mockLongpressText("text");
        // Generate the surrounding-text-available callback.
        // Surrounding text is gathered for longpress due to icing integration.
        generateTextSurroundingSelectionAvailable();

        Assert.assertEquals(mPanelManager.getRequestPanelShowCount(), 1);
        Assert.assertEquals(mPanelManager.getPanelHideCount(), 0);
        Assert.assertEquals(
                mContextualSearchManager.getSelectionController().getSelectedText(), "text");

        // Fake tap on non-text.
        mockTapEmptySpace();

        Assert.assertEquals(mPanelManager.getRequestPanelShowCount(), 1);
        Assert.assertEquals(mPanelManager.getPanelHideCount(), 1);
        Assert.assertNull(mContextualSearchManager.getSelectionController().getSelectedText());
    }

    /**
     * Tests that a Tap gesture followed by tapping empty space closes the panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTextTapFollowedByNonTextTap() {
        Assert.assertEquals(mPanelManager.getRequestPanelShowCount(), 0);

        // Fake a Tap event.
        mockTapText("text");
        // Generate the surrounding-text-available callback.
        generateTextSurroundingSelectionAvailable();
        // Right now the tap-processing sequence will stall at selectWordAroundCaret, so we need
        // to prod it forward by generating an ACK:
        generateSelectWordAroundCaretAck();
        Assert.assertEquals(mPanelManager.getRequestPanelShowCount(), 1);
        Assert.assertEquals(mPanelManager.getPanelHideCount(), 0);
    }

    /**
     * Tests that a Tap gesture processing is robust even when the selection somehow gets cleared
     * during that process.  This tests a failure-case found in crbug.com/728644.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Features.DisableFeatures("ContextualSearchLongpressResolve")
    public void testTapProcessIsRobustWhenSelectionGetsCleared() {
        Assert.assertEquals(mPanelManager.getRequestPanelShowCount(), 0);

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
        Assert.assertEquals(mPanelManager.getRequestPanelShowCount(), 0);
        Assert.assertEquals(mPanelManager.getPanelHideCount(), 0);
    }
}
