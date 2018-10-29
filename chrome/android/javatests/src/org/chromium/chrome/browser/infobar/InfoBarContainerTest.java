// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.graphics.Rect;
import android.graphics.Region;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.TextView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Tests for the InfoBarContainer.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class InfoBarContainerTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String MESSAGE_TEXT = "Ding dong. Woof. Translate french? Bears!";

    private static final class TestListener implements SimpleConfirmInfoBarBuilder.Listener {
        public final CallbackHelper dismissedCallback = new CallbackHelper();
        public final CallbackHelper primaryButtonCallback = new CallbackHelper();
        public final CallbackHelper secondaryButtonCallback = new CallbackHelper();

        @Override
        public void onInfoBarDismissed() {
            dismissedCallback.notifyCalled();
        }

        @Override
        public boolean onInfoBarButtonClicked(boolean isPrimary) {
            if (isPrimary) {
                primaryButtonCallback.notifyCalled();
            } else {
                secondaryButtonCallback.notifyCalled();
            }
            return false;
        }
    }

    private InfoBarTestAnimationListener mListener;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        // Register for animation notifications
        InfoBarContainer container = mActivityTestRule.getInfoBarContainer();
        mListener =  new InfoBarTestAnimationListener();
        container.addAnimationListener(mListener);

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    // Adds an infobar to the currrent tab. Blocks until the infobar has been added.
    private TestListener addInfoBarToCurrentTab(final boolean expires)
            throws InterruptedException, TimeoutException {
        List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        int previousCount = infoBars.size();

        final TestListener testListener = new TestListener();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                SimpleConfirmInfoBarBuilder.create(mActivityTestRule.getActivity().getActivityTab(),
                        testListener, InfoBarIdentifier.TEST_INFOBAR, 0, MESSAGE_TEXT, null, null,
                        expires);
            }
        });
        mListener.addInfoBarAnimationFinished("InfoBar not added.");

        // Verify it's really there.
        Assert.assertEquals(previousCount + 1, infoBars.size());
        TextView message =
                (TextView) infoBars.get(previousCount).getView().findViewById(R.id.infobar_message);
        Assert.assertEquals(MESSAGE_TEXT, message.getText().toString());

        return testListener;
    }

    /**
     * Dismisses the infobar by directly telling the infobar its close button was clicked.
     * Blocks until it's been removed.
     */
    private void dismissInfoBar(final InfoBar infoBar, TestListener listener)
            throws Exception {
        Assert.assertEquals(0, listener.dismissedCallback.getCallCount());
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                infoBar.onCloseButtonClicked();
            }
        });
        mListener.removeInfoBarAnimationFinished("InfoBar not removed.");
        listener.dismissedCallback.waitForCallback(0, 1);
        Assert.assertEquals(0, listener.primaryButtonCallback.getCallCount());
        Assert.assertEquals(0, listener.secondaryButtonCallback.getCallCount());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    /**
     * Verifies that infobars added from Java expire or not as expected.
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testInfoBarExpiration() throws Exception {
        // First add an infobar that expires.
        TestListener infobarListener = addInfoBarToCurrentTab(true);

        // Now navigate, it should expire.
        mActivityTestRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/google.html"));
        mListener.removeInfoBarAnimationFinished("InfoBar not removed.");
        Assert.assertTrue(mActivityTestRule.getInfoBars().isEmpty());
        Assert.assertEquals(0, infobarListener.dismissedCallback.getCallCount());
        Assert.assertEquals(0, infobarListener.primaryButtonCallback.getCallCount());
        Assert.assertEquals(0, infobarListener.secondaryButtonCallback.getCallCount());

        // Now test a non-expiring infobar.
        TestListener persistentListener = addInfoBarToCurrentTab(false);

        // Navigate, it should still be there.
        mActivityTestRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/about.html"));
        List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        Assert.assertEquals(1, infoBars.size());
        TextView message =
                (TextView) infoBars.get(0).getView().findViewById(R.id.infobar_message);
        Assert.assertEquals(MESSAGE_TEXT, message.getText().toString());

        // Close the infobar.
        dismissInfoBar(infoBars.get(0), persistentListener);
    }

    // Define function to pass parameter to Runnable to be used in testInfoBarExpirationNoPrerender.
    private Runnable setNetworkPredictionOptions(final boolean networkPredictionEnabled) {
        return new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge.getInstance().setNetworkPredictionEnabled(
                        networkPredictionEnabled);
            }
        };
    }

    /**
     * Same as testInfoBarExpiration but with prerender turned-off.
     * The behavior when prerender is on/off is different as in the prerender case the infobars are
     * added when we swap tabs.
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testInfoBarExpirationNoPrerender() throws Exception {
        // Save prediction preference.
        boolean networkPredictionEnabled =
                ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return PrefServiceBridge.getInstance().getNetworkPredictionEnabled();
                    }
                });
        try {
            ThreadUtils.runOnUiThreadBlocking(setNetworkPredictionOptions(false));
            testInfoBarExpiration();
        } finally {
            // Make sure we restore prediction preference.
            ThreadUtils.runOnUiThreadBlocking(
                    setNetworkPredictionOptions(networkPredictionEnabled));
        }
    }

    /**
     * Tests that adding and then immediately dismissing an infobar works as expected (and does not
     * assert).
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testQuickAddOneAndDismiss() throws Exception {
        final TestListener infobarListener = addInfoBarToCurrentTab(false);
        Assert.assertEquals(1, mActivityTestRule.getInfoBars().size());
        final InfoBar infoBar = mActivityTestRule.getInfoBars().get(0);
        dismissInfoBar(infoBar, infobarListener);
        Assert.assertTrue(mActivityTestRule.getInfoBars().isEmpty());
    }

    /**
     * Tests that we don't assert when a tab is getting closed while an infobar is being shown and
     * had been removed.
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testCloseTabOnAdd() throws Exception {
        mActivityTestRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/google.html"));

        final TestListener infobarListener = addInfoBarToCurrentTab(false);
        Assert.assertEquals(1, mActivityTestRule.getInfoBars().size());
        final InfoBar infoBar = mActivityTestRule.getInfoBars().get(0);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertEquals(0, infobarListener.dismissedCallback.getCallCount());
                infoBar.onCloseButtonClicked();
                mActivityTestRule.getActivity().getTabModelSelector().closeTab(
                        mActivityTestRule.getActivity().getActivityTab());
            }
        });

        infobarListener.dismissedCallback.waitForCallback(0, 1);
        Assert.assertEquals(0, infobarListener.primaryButtonCallback.getCallCount());
        Assert.assertEquals(0, infobarListener.secondaryButtonCallback.getCallCount());
    }

    /**
     * Tests that the x button in the infobar does close the infobar and that the event is not
     * propagated to the ContentView.
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testCloseButton() throws Exception {
        mActivityTestRule.loadUrl(
                mTestServer.getURL("/chrome/test/data/android/click_listener.html"));
        TestListener infobarListener = addInfoBarToCurrentTab(false);

        // Now press the close button.
        Assert.assertEquals(0, infobarListener.dismissedCallback.getCallCount());
        Assert.assertTrue("Close button wasn't found",
                InfoBarUtil.clickCloseButton(mActivityTestRule.getInfoBars().get(0)));
        mListener.removeInfoBarAnimationFinished("Infobar not removed.");
        infobarListener.dismissedCallback.waitForCallback(0, 1);
        Assert.assertEquals(0, infobarListener.primaryButtonCallback.getCallCount());
        Assert.assertEquals(0, infobarListener.secondaryButtonCallback.getCallCount());

        // The page should not have received the click.
        Assert.assertTrue("The page recieved the click.",
                !Boolean.parseBoolean(
                        mActivityTestRule.runJavaScriptCodeInCurrentTab("wasClicked")));
    }

    /**
     * Tests that adding and removing correctly manages the transparent region, which allows for
     * optimizations in SurfaceFlinger (less overlays).
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testAddAndDismissSurfaceFlingerOverlays() throws Exception {
        final ViewGroup decorView =
                (ViewGroup) mActivityTestRule.getActivity().getWindow().getDecorView();
        final InfoBarContainer infoBarContainer = mActivityTestRule.getInfoBarContainer();

        // Detect layouts. Note this doesn't actually need to be atomic (just final).
        final AtomicInteger layoutCount = new AtomicInteger();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                decorView.getViewTreeObserver().addOnGlobalLayoutListener(
                        new ViewTreeObserver.OnGlobalLayoutListener() {
                            @Override
                            public void onGlobalLayout() {
                                layoutCount.incrementAndGet();
                            }
                        });
            }
        });

        // First add an infobar.
        TestListener infobarListener = addInfoBarToCurrentTab(false);
        Assert.assertEquals(1, mActivityTestRule.getInfoBars().size());
        final InfoBar infoBar = mActivityTestRule.getInfoBars().get(0);

        // A layout must occur to recalculate the transparent region.
        CriteriaHelper.pollUiThread(
                new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return layoutCount.get() > 0;
                    }
                });

        final Rect fullDisplayFrame = new Rect();
        final Rect fullDisplayFrameMinusContainer = new Rect();
        final Rect containerDisplayFrame = new Rect();

        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                decorView.getWindowVisibleDisplayFrame(fullDisplayFrame);
                decorView.getWindowVisibleDisplayFrame(fullDisplayFrameMinusContainer);
                fullDisplayFrameMinusContainer.bottom -= infoBarContainer.getHeight();
                int windowLocation[] = new int[2];
                infoBarContainer.getLocationInWindow(windowLocation);
                containerDisplayFrame.set(
                        windowLocation[0],
                        windowLocation[1],
                        windowLocation[0] + infoBarContainer.getWidth(),
                        windowLocation[1] + infoBarContainer.getHeight());

                // The InfoBarContainer subtracts itself from the transparent region.
                Region transparentRegion = new Region(fullDisplayFrame);
                infoBarContainer.gatherTransparentRegion(transparentRegion);
                Assert.assertEquals(
                        "Values did not match. Expected: " + transparentRegion.getBounds()
                                + ", actual: " + fullDisplayFrameMinusContainer,
                        transparentRegion.getBounds(), fullDisplayFrameMinusContainer);
            }
        });

        // Now remove the infobar.
        layoutCount.set(0);
        dismissInfoBar(infoBar, infobarListener);

        // A layout must occur to recalculate the transparent region.
        CriteriaHelper.pollUiThread(
                new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return layoutCount.get() > 0;
                    }
                });

        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // The InfoBarContainer should no longer be subtracted from the transparent region.
                // We really want assertTrue(transparentRegion.contains(containerDisplayFrame)),
                // but region doesn't have 'contains(Rect)', so we invert the test. So, the old
                // container rect can't touch the bounding rect of the non-transparent region).
                Region transparentRegion = new Region();
                decorView.gatherTransparentRegion(transparentRegion);
                Region opaqueRegion = new Region(fullDisplayFrame);
                opaqueRegion.op(transparentRegion, Region.Op.DIFFERENCE);
                Assert.assertFalse("Opaque region " + opaqueRegion.getBounds()
                                + " should not intersect " + containerDisplayFrame,
                        opaqueRegion.getBounds().intersect(containerDisplayFrame));
            }
        });

        // Additional manual test that this is working:
        // - adb shell dumpsys SurfaceFlinger
        // - Observe that Clank's overlay size changes (or disappears if URLbar is also gone).
    }
}
