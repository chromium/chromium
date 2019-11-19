// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.portals;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.View;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests for the chrome/ layer support of the HTML portal element.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=Portals",
        "enable-blink-features=OverscrollCustomization"})
public class PortalsTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    private class TabContentsSwapObserver extends EmptyTabObserver {
        private final CallbackHelper mCallbackHelper;

        public TabContentsSwapObserver() {
            mCallbackHelper = new CallbackHelper();
        }

        public CallbackHelper getCallbackHelper() {
            return mCallbackHelper;
        }

        @Override
        public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
            mCallbackHelper.notifyCalled();
        }
    }

    /**
     * Used to observe and wait for the first layout after the tab's WebContents is swapped.
     */
    private class LayoutAfterTabContentsSwappedObserver
            extends EmptyTabObserver implements View.OnLayoutChangeListener {
        private final CallbackHelper mCallbackHelper;

        public LayoutAfterTabContentsSwappedObserver(Tab tab) {
            tab.addObserver(this);
            mCallbackHelper = new CallbackHelper();
        }

        @Override
        public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
            tab.getContentView().addOnLayoutChangeListener(this);
        }

        public CallbackHelper getCallbackHelper() {
            return mCallbackHelper;
        }

        @Override
        public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
                int oldTop, int oldRight, int oldBottom) {
            v.removeOnLayoutChangeListener(this);
            mCallbackHelper.notifyCalled();
        }
    }

    private void executeScriptAndAwaitSwap(Tab tab, String code) throws Exception {
        TabContentsSwapObserver swapObserver = new TabContentsSwapObserver();
        CallbackHelper swapWaiter = swapObserver.getCallbackHelper();
        tab.addObserver(swapObserver);

        int currSwapCount = swapWaiter.getCallCount();
        JavaScriptUtils.executeJavaScript(tab.getWebContents(), code);
        swapWaiter.waitForCallback(currSwapCount, 1);
    }

    /**
     * Tests that a portal can be activated and have its contents swapped in to its embedder's tab.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testActivate() throws Exception {
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(
                "/chrome/test/data/android/portals/portal-to-basic-content.html"));

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        final WebContents embedderContents = tab.getWebContents();
        Assert.assertNotNull(embedderContents);

        executeScriptAndAwaitSwap(tab, "activatePortal();");

        final WebContents portalContents = tab.getWebContents();
        Assert.assertNotNull(portalContents);
        Assert.assertNotSame(embedderContents, portalContents);
    }

    /**
     * Tests that a portal can be activated and adopt the embedder's contents.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testAdoptPredecessor() throws Exception {
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/portals/predecessor-adoption.html"));

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        executeScriptAndAwaitSwap(tab, "activatePortal();");
        JavaScriptUtils.runJavascriptWithAsyncResult(tab.getWebContents(),
                "pingPredecessor().then(() => { domAutomationController.send(true); });");
    }

    /**
     * Tests that an adopted predecessor can be activated and adopt its portal back.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testReactivatePredecessor() throws Exception {
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/portals/predecessor-adoption.html"));

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        executeScriptAndAwaitSwap(tab, "activatePortal();");
        executeScriptAndAwaitSwap(tab, "reactivatePredecessor();");
    }

    /**
     * Tests that an adopted predecessor can be destroyed.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testRemovePredecessor() throws Exception {
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/portals/predecessor-adoption.html"));

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        executeScriptAndAwaitSwap(tab, "activatePortal();");
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "removePredecessor();");
    }

    /**
     * Tests that a previously activated portal can be destroyed.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testRemovePreviouslyActivePortal() throws Exception {
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/portals/predecessor-adoption.html"));

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        executeScriptAndAwaitSwap(tab, "activatePortal();");
        executeScriptAndAwaitSwap(tab, "reactivatePredecessor();");
        JavaScriptUtils.executeJavaScriptAndWaitForResult(tab.getWebContents(), "removePortal();");
    }

    /**
     * Tests that a drag that started in the predecessor page causes a scroll in the activated page
     * after a scroll triggered activation.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testTouchTransfer() throws Exception {
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(
                "/chrome/test/data/android/portals/touch-transfer.html?event=overscroll"));

        ChromeActivity activity = mActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        View contentView = tab.getContentView();
        LayoutAfterTabContentsSwappedObserver layoutObserver =
                new LayoutAfterTabContentsSwappedObserver(tab);
        CallbackHelper layoutWaiter = layoutObserver.getCallbackHelper();
        int currLayoutCount = layoutWaiter.getCallCount();

        int dragStartX = 30;
        int dragStartY = contentView.getHeight() / 2;
        int dragPauseY = dragStartY - 30;
        int dragEndX = dragStartX;
        int dragEndY = 30;
        long downTime = System.currentTimeMillis();

        // Initial drag to trigger activation.
        TouchCommon.dragStart(activity, dragStartX, dragStartY, downTime);
        TouchCommon.dragTo(activity, dragStartX, dragEndX, dragStartY, dragPauseY, 100, downTime);

        // Wait for the first layout after tab contents are swapped. This is needed as touch events
        // sent before the first layout are dropped.
        layoutWaiter.waitForCallback(currLayoutCount, 1);

        // Continue and finish drag.
        TouchCommon.dragTo(activity, dragStartX, dragEndX, dragPauseY, dragEndY, 100, downTime);
        TouchCommon.dragEnd(activity, dragEndX, dragEndY, downTime);

        WebContents contents = mActivityTestRule.getWebContents();
        Assert.assertTrue(Coordinates.createFor(contents).getScrollYPixInt() > 0);
    }

    /**
     * Tests that touch is transferred after triggering portal activation on touchstart.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    @DisabledTest // Disabled due to flakiness. See https://crbug.com/1024850
    public void testTouchTransferAfterTouchStartActivate() throws Exception {
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(
                "/chrome/test/data/android/portals/touch-transfer.html?event=touchstart"));

        ChromeActivity activity = mActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        View contentView = tab.getContentView();
        LayoutAfterTabContentsSwappedObserver layoutObserver =
                new LayoutAfterTabContentsSwappedObserver(tab);
        CallbackHelper layoutWaiter = layoutObserver.getCallbackHelper();
        int currLayoutCount = layoutWaiter.getCallCount();

        int dragStartX = 30;
        int dragStartY = contentView.getHeight() / 2;
        int dragEndX = dragStartX;
        int dragEndY = 30;
        long downTime = System.currentTimeMillis();

        // Initial touch to trigger activation.
        TouchCommon.dragStart(activity, dragStartX, dragStartY, downTime);

        // Wait for the first layout after tab contents are swapped. This is needed as touch events
        // sent before the first layout are dropped.
        layoutWaiter.waitForCallback(currLayoutCount, 1);

        // Continue and finish drag.
        TouchCommon.dragTo(activity, dragStartX, dragEndX, dragStartY, dragEndY, 100, downTime);
        TouchCommon.dragEnd(activity, dragEndX, dragEndY, downTime);

        WebContents contents = mActivityTestRule.getWebContents();
        Assert.assertTrue(Coordinates.createFor(contents).getScrollYPixInt() > 0);
    }

    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testTouchTransferAfterReactivation() throws Exception {
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(
                "/chrome/test/data/android/portals/touch-transfer-after-reactivation.html"));

        ChromeActivity activity = mActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        View contentView = tab.getContentView();

        TabContentsSwapObserver swapObserver = new TabContentsSwapObserver();
        CallbackHelper swapWaiter = swapObserver.getCallbackHelper();
        tab.addObserver(swapObserver);
        int currSwapCount = swapWaiter.getCallCount();

        int dragStartX = 30;
        int dragStartY = contentView.getHeight() / 2;
        int dragEndX = dragStartX;
        int dragEndY = 30;
        long downTime = System.currentTimeMillis();

        // Initial touch to trigger activation.
        TouchCommon.dragStart(activity, dragStartX, dragStartY, downTime);

        // Wait for first activation.
        swapWaiter.waitForCallback(currSwapCount, 1);

        LayoutAfterTabContentsSwappedObserver layoutObserver =
                new LayoutAfterTabContentsSwappedObserver(tab);
        CallbackHelper layoutWaiter = layoutObserver.getCallbackHelper();
        int currLayoutCount = layoutWaiter.getCallCount();

        // Wait for the first layout after second activation (reactivation of predecessor).
        layoutWaiter.waitForCallback(currLayoutCount, 1);

        // Continue and finish drag.
        TouchCommon.dragTo(activity, dragStartX, dragEndX, dragStartY, dragEndY, 100, downTime);
        TouchCommon.dragEnd(activity, dragEndX, dragEndY, downTime);

        WebContents contents = mActivityTestRule.getWebContents();
        Assert.assertTrue(Coordinates.createFor(contents).getScrollYPixInt() > 0);
    }
}
