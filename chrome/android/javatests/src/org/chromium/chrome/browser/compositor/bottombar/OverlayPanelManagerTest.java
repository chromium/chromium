// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.OverlayPanelManagerObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.PanelPriority;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.concurrent.TimeoutException;

/**
 * Class responsible for testing the OverlayPanelManager.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class OverlayPanelManagerTest {
    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    // --------------------------------------------------------------------------------------------
    // MockOverlayPanel
    // --------------------------------------------------------------------------------------------

    /**
     * Mocks the ContextualSearchPanel, so it doesn't create WebContents.
     */
    private static class MockOverlayPanel extends OverlayPanel {
        private @PanelPriority int mPriority;
        private boolean mCanBeSuppressed;
        private ViewGroup mContainerView;
        private DynamicResourceLoader mResourceLoader;

        public MockOverlayPanel(Context context, LayoutUpdateHost updateHost,
                OverlayPanelManager panelManager, @PanelPriority int priority,
                boolean canBeSuppressed) {
            super(context, updateHost, panelManager);
            mPriority = priority;
            mCanBeSuppressed = canBeSuppressed;
        }

        @Override
        public void setContainerView(ViewGroup container) {
            super.setContainerView(container);
            mContainerView = container;
        }

        @Override
        public ViewGroup getContainerView() {
            return mContainerView;
        }

        @Override
        public void setDynamicResourceLoader(DynamicResourceLoader loader) {
            super.setDynamicResourceLoader(loader);
            mResourceLoader = loader;
        }

        public DynamicResourceLoader getDynamicResourceLoader() {
            return mResourceLoader;
        }

        @Override
        public OverlayPanelContent createNewOverlayPanelContent() {
            return new MockOverlayPanelContent();
        }

        @Override
        public @PanelPriority int getPriority() {
            return mPriority;
        }

        @Override
        public boolean canBeSuppressed() {
            return mCanBeSuppressed;
        }

        @Override
        protected void animatePanelTo(float height, long duration) {
            // Do not create animations for tests.
        }

        @Override
        public void closePanel(@StateChangeReason int reason, boolean animate) {
            // Immediately call onClosed rather than wait for animation to finish.
            onClosed(reason);
        }

        /**
         * Override creation and destruction of the WebContents as they rely on native methods.
         */
        private static class MockOverlayPanelContent extends OverlayPanelContent {
            public MockOverlayPanelContent() {
                super(null, null, null, false, 0);
            }

            @Override
            public void removeLastHistoryEntry(String url, long timeInMs) {}
        }
    }

    // --------------------------------------------------------------------------------------------
    // Test Suite
    // --------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testPanelRequestingShow() {
        Context context = InstrumentationRegistry.getTargetContext();

        OverlayPanelManager panelManager = new OverlayPanelManager();
        OverlayPanel panel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.MEDIUM, false);

        panel.requestPanelShow(StateChangeReason.UNKNOWN);

        Assert.assertTrue(panelManager.getActivePanel() == panel);
    }

    @Test
    @SmallTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testPanelClosed() {
        Context context = InstrumentationRegistry.getTargetContext();

        OverlayPanelManager panelManager = new OverlayPanelManager();
        OverlayPanel panel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.MEDIUM, false);

        panel.requestPanelShow(StateChangeReason.UNKNOWN);
        panel.closePanel(StateChangeReason.UNKNOWN, false);

        Assert.assertTrue(panelManager.getActivePanel() == null);
    }

    @Test
    @SmallTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testHighPrioritySuppressingLowPriority() {
        Context context = InstrumentationRegistry.getTargetContext();

        OverlayPanelManager panelManager = new OverlayPanelManager();
        OverlayPanel lowPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.LOW, false);
        OverlayPanel highPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.HIGH, false);

        lowPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        highPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);

        Assert.assertTrue(panelManager.getActivePanel() == highPriorityPanel);
    }

    @Test
    @SmallTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testSuppressedPanelRestored() {
        Context context = InstrumentationRegistry.getTargetContext();

        OverlayPanelManager panelManager = new OverlayPanelManager();
        OverlayPanel lowPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.LOW, true);
        OverlayPanel highPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.HIGH, false);

        lowPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        highPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        highPriorityPanel.closePanel(StateChangeReason.UNKNOWN, false);

        Assert.assertTrue(panelManager.getActivePanel() == lowPriorityPanel);
    }

    @Test
    @SmallTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testUnsuppressiblePanelNotRestored() {
        Context context = InstrumentationRegistry.getTargetContext();

        OverlayPanelManager panelManager = new OverlayPanelManager();
        OverlayPanel lowPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.LOW, false);
        OverlayPanel highPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.HIGH, false);

        lowPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        highPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        highPriorityPanel.closePanel(StateChangeReason.UNKNOWN, false);

        Assert.assertTrue(panelManager.getActivePanel() == null);
    }

    @Test
    @SmallTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testSuppressedPanelClosedBeforeRestore() {
        Context context = InstrumentationRegistry.getTargetContext();

        OverlayPanelManager panelManager = new OverlayPanelManager();
        OverlayPanel lowPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.LOW, true);
        OverlayPanel highPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.HIGH, false);

        lowPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        highPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        lowPriorityPanel.closePanel(StateChangeReason.UNKNOWN, false);
        highPriorityPanel.closePanel(StateChangeReason.UNKNOWN, false);

        Assert.assertEquals(null, panelManager.getActivePanel());
        Assert.assertEquals(0, panelManager.getSuppressedQueueSize());
    }

    @Test
    @SmallTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testSuppressedPanelPriority() {
        Context context = InstrumentationRegistry.getTargetContext();

        OverlayPanelManager panelManager = new OverlayPanelManager();
        OverlayPanel lowPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.LOW, true);
        OverlayPanel mediumPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.MEDIUM, true);
        OverlayPanel highPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.HIGH, false);

        // Only one panel is showing, should be medium priority.
        mediumPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        Assert.assertEquals(panelManager.getActivePanel(), mediumPriorityPanel);

        // High priority should have taken preciedence.
        highPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        Assert.assertEquals(panelManager.getActivePanel(), highPriorityPanel);

        // Low priority will be suppressed; high priority should still be showing.
        lowPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        Assert.assertEquals(panelManager.getActivePanel(), highPriorityPanel);

        // Start closing panels.
        highPriorityPanel.closePanel(StateChangeReason.UNKNOWN, false);

        // After high priority is closed, the medium priority panel should be visible.
        Assert.assertEquals(panelManager.getActivePanel(), mediumPriorityPanel);
        mediumPriorityPanel.closePanel(StateChangeReason.UNKNOWN, false);

        // Finally the low priority panel should be showing.
        Assert.assertEquals(panelManager.getActivePanel(), lowPriorityPanel);
        lowPriorityPanel.closePanel(StateChangeReason.UNKNOWN, false);

        // All panels are closed now.
        Assert.assertEquals(null, panelManager.getActivePanel());
        Assert.assertEquals(0, panelManager.getSuppressedQueueSize());
    }

    @Test
    @SmallTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testSuppressedPanelOrder() {
        Context context = InstrumentationRegistry.getTargetContext();

        OverlayPanelManager panelManager = new OverlayPanelManager();
        OverlayPanel lowPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.LOW, true);
        OverlayPanel mediumPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.MEDIUM, true);
        OverlayPanel highPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.HIGH, false);

        // Odd ordering for showing panels should still produce ordered suppression.
        highPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        lowPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        mediumPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);

        Assert.assertEquals(2, panelManager.getSuppressedQueueSize());

        // Start closing panels.
        highPriorityPanel.closePanel(StateChangeReason.UNKNOWN, false);

        // After high priority is closed, the medium priority panel should be visible.
        Assert.assertEquals(panelManager.getActivePanel(), mediumPriorityPanel);
        mediumPriorityPanel.closePanel(StateChangeReason.UNKNOWN, false);

        // Finally the low priority panel should be showing.
        Assert.assertEquals(panelManager.getActivePanel(), lowPriorityPanel);
        lowPriorityPanel.closePanel(StateChangeReason.UNKNOWN, false);

        // All panels are closed now.
        Assert.assertTrue(panelManager.getActivePanel() == null);
        Assert.assertEquals(0, panelManager.getSuppressedQueueSize());
    }

    @Test
    @SmallTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testLatePanelGetsNecessaryVars() {
        Context context = InstrumentationRegistry.getTargetContext();

        OverlayPanelManager panelManager = new OverlayPanelManager();
        MockOverlayPanel earlyPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.MEDIUM, true);

        // Set necessary vars before any other panels are registered in the manager.
        panelManager.setContainerView(new LinearLayout(InstrumentationRegistry.getTargetContext()));
        panelManager.setDynamicResourceLoader(new DynamicResourceLoader(0, null));

        MockOverlayPanel latePanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.MEDIUM, true);

        Assert.assertTrue(earlyPanel.getContainerView() == latePanel.getContainerView());
        Assert.assertTrue(
                earlyPanel.getDynamicResourceLoader() == latePanel.getDynamicResourceLoader());
    }

    @Test
    @SmallTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testManagerObserver() throws TimeoutException {
        Context context = InstrumentationRegistry.getTargetContext();

        final CallbackHelper shownHelper = new CallbackHelper();
        final CallbackHelper hiddenHelper = new CallbackHelper();

        OverlayPanelManager panelManager = new OverlayPanelManager();
        panelManager.addObserver(new OverlayPanelManagerObserver() {
            @Override
            public void onOverlayPanelShown() {
                shownHelper.notifyCalled();
            }

            @Override
            public void onOverlayPanelHidden() {
                hiddenHelper.notifyCalled();
            }
        });

        OverlayPanel lowPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.LOW, true);
        OverlayPanel highPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.HIGH, false);

        lowPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);

        // Wait for the panel to be shown.
        shownHelper.waitForCallback(0);

        highPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);

        // Wait for the low priority panel to be hidden and high priority panel to be shown.
        hiddenHelper.waitForCallback(0);
        shownHelper.waitForCallback(1);

        highPriorityPanel.closePanel(StateChangeReason.UNKNOWN, false);

        // Wait for the open panel to be closed and the supressed one to be re-shown.
        hiddenHelper.waitForCallback(1);
        shownHelper.waitForCallback(2);

        Assert.assertEquals(
                "Shown event should have been called three times.", 3, shownHelper.getCallCount());
        Assert.assertEquals(
                "Hidden event should have been called twice.", 2, hiddenHelper.getCallCount());
    }
}
