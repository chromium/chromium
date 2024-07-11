// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.graphics.Rect;
import android.graphics.Region;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.TextView;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsBridge;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesState;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.messages.infobar.SimpleConfirmInfoBarBuilder;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.components.infobars.InfoBar;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;

/** Tests for the InfoBarContainer. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class InfoBarContainerTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    // URL takes longer to load for batch tests where the activity is reused across rests
    private static final long EXTENDED_LOAD_TIMEOUT = 10L;
    private static final String MESSAGE_TEXT = "Ding dong. Woof. Translate french? Bears!";
    private static EmbeddedTestServer sTestServer = sActivityTestRule.getTestServer();

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

        @Override
        public boolean onInfoBarLinkClicked() {
            return false;
        }
    }

    private InfoBarTestAnimationListener mListener;

    @Before
    public void setUp() throws Exception {
        // Register for animation notifications
        InfoBarContainer container = sActivityTestRule.getInfoBarContainer();
        mListener = new InfoBarTestAnimationListener();
        ThreadUtils.runOnUiThreadBlocking(() -> container.addAnimationListener(mListener));
    }

    @After
    public void tearDown() {
        // Unregister animation notifications
        InfoBarContainer container = sActivityTestRule.getInfoBarContainer();
        if (container != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        container.removeAnimationListener(mListener);
                        InfoBarContainer.removeInfoBarContainerForTesting(
                                sActivityTestRule.getActivity().getActivityTab());
                    });
        }
    }

    // Adds an infobar to the currrent tab. Blocks until the infobar has been added.
    private TestListener addInfoBarToCurrentTab(final boolean expires) throws TimeoutException {
        List<InfoBar> infoBars = sActivityTestRule.getInfoBars();
        int previousCount = infoBars.size();

        final TestListener testListener = new TestListener();
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    SimpleConfirmInfoBarBuilder.create(
                            sActivityTestRule.getActivity().getActivityTab().getWebContents(),
                            testListener,
                            InfoBarIdentifier.TEST_INFOBAR,
                            null,
                            0,
                            MESSAGE_TEXT,
                            null,
                            null,
                            null,
                            expires);
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
     * Dismisses the infobar by directly telling the infobar its close button was clicked. Blocks
     * until it's been removed.
     */
    private void dismissInfoBar(final InfoBar infoBar, TestListener listener) throws Exception {
        Assert.assertEquals(0, listener.dismissedCallback.getCallCount());
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        new Runnable() {
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

    /** Verifies that infobars added from Java expire or not as expected. */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testInfoBarExpiration() throws Exception {
        // First add an infobar that expires.
        TestListener infobarListener = addInfoBarToCurrentTab(true);

        // Now navigate, it should expire.
        sActivityTestRule.loadUrl(
                sTestServer.getURL("/chrome/test/data/android/google.html"), EXTENDED_LOAD_TIMEOUT);
        mListener.removeInfoBarAnimationFinished("InfoBar not removed.");
        Assert.assertTrue(sActivityTestRule.getInfoBars().isEmpty());
        Assert.assertEquals(0, infobarListener.dismissedCallback.getCallCount());
        Assert.assertEquals(0, infobarListener.primaryButtonCallback.getCallCount());
        Assert.assertEquals(0, infobarListener.secondaryButtonCallback.getCallCount());

        // Now test a non-expiring infobar.
        TestListener persistentListener = addInfoBarToCurrentTab(false);

        // Navigate, it should still be there.
        sActivityTestRule.loadUrl(sTestServer.getURL("/chrome/test/data/android/about.html"));
        List<InfoBar> infoBars = sActivityTestRule.getInfoBars();
        Assert.assertEquals(1, infoBars.size());
        TextView message = (TextView) infoBars.get(0).getView().findViewById(R.id.infobar_message);
        Assert.assertEquals(MESSAGE_TEXT, message.getText().toString());

        // Close the infobar.
        dismissInfoBar(infoBars.get(0), persistentListener);
    }

    // Define function to pass parameter to Runnable to be used in testInfoBarExpirationNoPrerender.
    private Runnable setNetworkPredictionOptions(final boolean networkPredictionEnabled) {
        return () -> {
            if (networkPredictionEnabled) {
                PreloadPagesSettingsBridge.setState(
                        ProfileManager.getLastUsedRegularProfile(),
                        PreloadPagesState.STANDARD_PRELOADING);
            } else {
                PreloadPagesSettingsBridge.setState(
                        ProfileManager.getLastUsedRegularProfile(),
                        PreloadPagesState.NO_PRELOADING);
            }
        };
    }

    /**
     * Same as testInfoBarExpiration but with prerender turned-off. The behavior when prerender is
     * on/off is different as in the prerender case the infobars are added when we swap tabs.
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testInfoBarExpirationNoPrerender() throws Exception {
        // Save prediction preference.
        boolean networkPredictionEnabled =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                PreloadPagesSettingsBridge.getState(
                                                ProfileManager.getLastUsedRegularProfile())
                                        != PreloadPagesState.NO_PRELOADING);
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
        Assert.assertEquals(1, sActivityTestRule.getInfoBars().size());
        final InfoBar infoBar = sActivityTestRule.getInfoBars().get(0);
        dismissInfoBar(infoBar, infobarListener);
        Assert.assertTrue(sActivityTestRule.getInfoBars().isEmpty());
    }

    /**
     * Tests that we don't assert when a tab is getting closed while an infobar is being shown and
     * had been removed.
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testCloseTabOnAdd() throws Exception {
        sActivityTestRule.loadUrl(sTestServer.getURL("/chrome/test/data/android/google.html"));

        final TestListener infobarListener = addInfoBarToCurrentTab(false);
        Assert.assertEquals(1, sActivityTestRule.getInfoBars().size());
        final InfoBar infoBar = sActivityTestRule.getInfoBars().get(0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(0, infobarListener.dismissedCallback.getCallCount());
                    infoBar.onCloseButtonClicked();
                    sActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .closeTab(sActivityTestRule.getActivity().getActivityTab());
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
        sActivityTestRule.loadUrl(
                sTestServer.getURL("/chrome/test/data/android/click_listener.html"));
        TestListener infobarListener = addInfoBarToCurrentTab(false);

        // Now press the close button.
        Assert.assertEquals(0, infobarListener.dismissedCallback.getCallCount());
        Assert.assertTrue(
                "Close button wasn't found",
                InfoBarUtil.clickCloseButton(sActivityTestRule.getInfoBars().get(0)));
        mListener.removeInfoBarAnimationFinished("Infobar not removed.");
        infobarListener.dismissedCallback.waitForCallback(0, 1);
        Assert.assertEquals(0, infobarListener.primaryButtonCallback.getCallCount());
        Assert.assertEquals(0, infobarListener.secondaryButtonCallback.getCallCount());

        // The page should not have received the click.
        Assert.assertTrue(
                "The page recieved the click.",
                !Boolean.parseBoolean(
                        sActivityTestRule.runJavaScriptCodeInCurrentTab("wasClicked")));
    }

    /**
     * Tests that adding and removing correctly manages the transparent region, which allows for
     * optimizations in SurfaceFlinger (less overlays).
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    @RequiresRestart("crbug.com/1242720")
    public void testAddAndDismissSurfaceFlingerOverlays() throws Exception {
        final ViewGroup decorView =
                (ViewGroup) sActivityTestRule.getActivity().getWindow().getDecorView();
        final InfoBarContainer infoBarContainer = sActivityTestRule.getInfoBarContainer();
        final InfoBarContainerView infoBarContainerView =
                infoBarContainer.getContainerViewForTesting();

        Assert.assertNotNull("InfoBarContainerView should not be null.", infoBarContainerView);

        // Detect layouts. Note this doesn't actually need to be atomic (just final).
        final AtomicInteger layoutCount = new AtomicInteger();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        new Runnable() {
                            @Override
                            public void run() {
                                decorView
                                        .getViewTreeObserver()
                                        .addOnGlobalLayoutListener(
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
        Assert.assertEquals(1, sActivityTestRule.getInfoBars().size());
        final InfoBar infoBar = sActivityTestRule.getInfoBars().get(0);

        // A layout must occur to recalculate the transparent region.
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(layoutCount.get(), Matchers.greaterThan(0)));

        final Rect fullDisplayFrame = new Rect();
        final Rect fullDisplayFrameMinusContainer = new Rect();
        final Rect containerDisplayFrame = new Rect();

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        new Runnable() {
                            @Override
                            public void run() {
                                decorView.getWindowVisibleDisplayFrame(fullDisplayFrame);
                                decorView.getWindowVisibleDisplayFrame(
                                        fullDisplayFrameMinusContainer);
                                fullDisplayFrameMinusContainer.bottom -=
                                        infoBarContainerView.getHeight();
                                int windowLocation[] = new int[2];
                                infoBarContainerView.getLocationInWindow(windowLocation);
                                containerDisplayFrame.set(
                                        windowLocation[0],
                                        windowLocation[1],
                                        windowLocation[0] + infoBarContainerView.getWidth(),
                                        windowLocation[1] + infoBarContainerView.getHeight());

                                // The InfoBarContainer subtracts itself from the transparent
                                // region.
                                Region transparentRegion = new Region(fullDisplayFrame);
                                infoBarContainerView.gatherTransparentRegion(transparentRegion);
                                Assert.assertEquals(
                                        "Values did not match. Expected: "
                                                + transparentRegion.getBounds()
                                                + ", actual: "
                                                + fullDisplayFrameMinusContainer,
                                        transparentRegion.getBounds(),
                                        fullDisplayFrameMinusContainer);
                            }
                        });

        // Now remove the infobar.
        layoutCount.set(0);
        dismissInfoBar(infoBar, infobarListener);

        // A layout must occur to recalculate the transparent region.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(layoutCount.get(), Matchers.greaterThan(0));
                    // The InfoBarContainer should no longer be subtracted from the transparent
                    // region.
                    // We really want assertTrue(transparentRegion.contains(containerDisplayFrame)),
                    // but region doesn't have 'contains(Rect)', so we invert the test. So, the old
                    // container rect can't touch the bounding rect of the non-transparent region).
                    Region transparentRegion = new Region();
                    decorView.gatherTransparentRegion(transparentRegion);
                    Region opaqueRegion = new Region(fullDisplayFrame);
                    opaqueRegion.op(transparentRegion, Region.Op.DIFFERENCE);
                    Criteria.checkThat(
                            "Opaque region "
                                    + opaqueRegion.getBounds()
                                    + " should not intersect "
                                    + containerDisplayFrame,
                            opaqueRegion.getBounds().intersect(containerDisplayFrame),
                            Matchers.is(false));
                });

        // Additional manual test that this is working:
        // - adb shell dumpsys SurfaceFlinger
        // - Observe that Clank's overlay size changes (or disappears if URLbar is also gone).
    }

    /** Tests that infobar container view hides when browser control is offset. */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testSyncWithBrowserControl() throws Exception {
        addInfoBarToCurrentTab(false);
        Assert.assertEquals(1, sActivityTestRule.getInfoBars().size());
        final InfoBar infoBar = sActivityTestRule.getInfoBars().get(0);
        Assert.assertEquals(0, infoBar.getView().getTranslationY(), /* delta= */ 0.1);

        InfoBarContainer infoBarContainer = sActivityTestRule.getInfoBarContainer();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    infoBarContainer
                            .getContainerViewForTesting()
                            .onControlsOffsetChanged(-100, 100, 0, 0, false, false);
                });
        Assert.assertNotEquals(
                0,
                infoBarContainer.getContainerViewForTesting().getTranslationY(),
                /* delta= */ 0.1);
    }
}
