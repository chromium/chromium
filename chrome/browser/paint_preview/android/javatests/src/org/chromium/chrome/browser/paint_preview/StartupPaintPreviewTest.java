// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import static org.chromium.base.test.util.Batch.PER_CLASS;
import static org.chromium.chrome.browser.paint_preview.TabbedPaintPreviewTest.assertAttachedAndShown;

import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabService;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.paintpreview.player.PlayerManager;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.ExecutionException;

/**
 * Tests for the {@link TabbedPaintPreview} class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(PER_CLASS)
public class StartupPaintPreviewTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();
    // Tell R8 not to break the ability to mock the class.
    @Mock
    private static PaintPreviewTabService sUnused;

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    private static final String TEST_URL = "/chrome/test/data/android/about.html";

    @BeforeClass
    public static void setUp() {
        PaintPreviewTabService mockService = Mockito.mock(PaintPreviewTabService.class);
        Mockito.doReturn(true).when(mockService).hasCaptureForTab(Mockito.anyInt());
        TabbedPaintPreview.overridePaintPreviewTabServiceForTesting(mockService);
        PlayerManager.overrideCompositorDelegateFactoryForTesting(
                new TabbedPaintPreviewTest.TestCompositorDelegateFactory());
    }

    @AfterClass
    public static void tearDown() {
        PlayerManager.overrideCompositorDelegateFactoryForTesting(null);
        TabbedPaintPreview.overridePaintPreviewTabServiceForTesting(null);
    }

    @Before
    public void setup() {
        sActivityTestRule.loadUrl(sActivityTestRule.getTestServer().getURL(TEST_URL));
    }

    /**
     * Tests that StartupPaintPreview is displayed correctly if a paint preview for the current tab
     * has been captured before.
     */
    @Test
    @MediumTest
    public void testDisplayedCorrectly() throws ExecutionException {
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        StartupPaintPreview startupPaintPreview = TestThreadUtils.runOnUiThreadBlocking(
                () -> new StartupPaintPreview(tab, null, null, null));
        TabbedPaintPreview tabbedPaintPreview =
                TestThreadUtils.runOnUiThreadBlocking(() -> TabbedPaintPreview.get(tab));
        showAndWaitForInflation(startupPaintPreview, tabbedPaintPreview, null);
    }

    @Test
    @MediumTest
    public void testSnackbarShow() throws ExecutionException, InterruptedException {
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        StartupPaintPreview startupPaintPreview = TestThreadUtils.runOnUiThreadBlocking(
                () -> new StartupPaintPreview(tab, null, null, null));
        TabbedPaintPreview tabbedPaintPreview =
                TestThreadUtils.runOnUiThreadBlocking(() -> TabbedPaintPreview.get(tab));
        showAndWaitForInflation(startupPaintPreview, tabbedPaintPreview, null);

        // Snackbar should appear on user frustration. It currently happens when users taps 3 times,
        // or when users longpress.
        SnackbarManager snackbarManager = sActivityTestRule.getActivity().getSnackbarManager();
        assertSnackbarVisibility(snackbarManager, false);
        View view = tabbedPaintPreview.getViewForTesting();

        // First tap.
        simulateTap(view, view.getWidth() / 2f, view.getHeight() / 2f);
        assertSnackbarVisibility(snackbarManager, false);
        // Second tap.
        simulateTap(view, view.getWidth() / 2f, view.getHeight() / 2f);
        assertSnackbarVisibility(snackbarManager, false);
        // Third tap.
        simulateTap(view, view.getWidth() / 2f, view.getHeight() / 2f);
        assertSnackbarVisibility(snackbarManager, true);

        TestThreadUtils.runOnUiThreadBlocking(snackbarManager::dismissAllSnackbars);
        assertSnackbarVisibility(snackbarManager, false);

        // Simulate long press.
        simulateLongPress(view, view.getWidth() / 2f, view.getHeight() / 2f);
        assertSnackbarVisibility(snackbarManager, true);
    }

    /**
     * Tests that the paint preview is removed when certain conditions are met.
     */
    @Test
    @MediumTest
    public void testRemoveOnFirstMeaningfulPaint() throws ExecutionException {
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        StartupPaintPreview startupPaintPreview = TestThreadUtils.runOnUiThreadBlocking(
                () -> new StartupPaintPreview(tab, null, null, null));
        TabbedPaintPreview tabbedPaintPreview =
                TestThreadUtils.runOnUiThreadBlocking(() -> TabbedPaintPreview.get(tab));
        CallbackHelper dismissCallback = new CallbackHelper();

        // Should be removed on FMP signal.
        showAndWaitForInflation(startupPaintPreview, tabbedPaintPreview, dismissCallback);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> startupPaintPreview.onWebContentsFirstMeaningfulPaint(tab.getWebContents()));
        assertAttachedAndShown(tabbedPaintPreview, false, false);
        Assert.assertEquals(
                "Dismiss callback should have been called.", 1, dismissCallback.getCallCount());
    }

    /**
     * Tests that the paint preview is removed when offline page is shown.
     */
    @Test
    @MediumTest
    public void testRemoveOnOfflinePage() throws ExecutionException {
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        StartupPaintPreview startupPaintPreview = TestThreadUtils.runOnUiThreadBlocking(
                () -> new StartupPaintPreview(tab, null, null, null));
        TabbedPaintPreview tabbedPaintPreview =
                TestThreadUtils.runOnUiThreadBlocking(() -> TabbedPaintPreview.get(tab));
        // Offline page callback always returns true.
        startupPaintPreview.setIsOfflinePage(() -> true);
        CallbackHelper dismissCallback = new CallbackHelper();

        showAndWaitForInflation(startupPaintPreview, tabbedPaintPreview, dismissCallback);
        assertAttachedAndShown(tabbedPaintPreview, true, true);
        // Should be removed on PageLoadFinished signal.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            startupPaintPreview.getTabObserverForTesting().onPageLoadFinished(tab, null);
        });
        assertAttachedAndShown(tabbedPaintPreview, false, false);
        Assert.assertEquals(
                "Dismiss callback should have been called.", 1, dismissCallback.getCallCount());
    }

    /**
     * Tests that the paint preview is removed when certain conditions are met.
     */
    @Test
    @MediumTest
    public void testRemoveOnSnackbarClick() throws ExecutionException, InterruptedException {
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        StartupPaintPreview startupPaintPreview = TestThreadUtils.runOnUiThreadBlocking(
                () -> new StartupPaintPreview(tab, null, null, null));
        TabbedPaintPreview tabbedPaintPreview =
                TestThreadUtils.runOnUiThreadBlocking(() -> TabbedPaintPreview.get(tab));
        CallbackHelper dismissCallback = new CallbackHelper();

        // Should be removed on SnackBar click.
        showAndWaitForInflation(startupPaintPreview, tabbedPaintPreview, dismissCallback);
        SnackbarManager snackbarManager = sActivityTestRule.getActivity().getSnackbarManager();
        View view = tabbedPaintPreview.getViewForTesting();
        simulateLongPress(view, view.getWidth() / 2f, view.getHeight() / 2f);
        assertSnackbarVisibility(snackbarManager, true);
        TestThreadUtils.runOnUiThreadBlocking(()->
                snackbarManager.getCurrentSnackbarForTesting().getController().onAction(null));
        assertAttachedAndShown(tabbedPaintPreview, false, false);
        Assert.assertEquals(
                "Dismiss callback should have been called.", 1, dismissCallback.getCallCount());
    }

    /**
     * Tests that the paint preview is removed when certain conditions are met.
     */
    @Test
    @MediumTest
    public void testRemoveOnNavigation() throws ExecutionException {
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        StartupPaintPreview startupPaintPreview = TestThreadUtils.runOnUiThreadBlocking(
                () -> new StartupPaintPreview(tab, null, null, null));
        TabbedPaintPreview tabbedPaintPreview =
                TestThreadUtils.runOnUiThreadBlocking(() -> TabbedPaintPreview.get(tab));
        CallbackHelper dismissCallback = new CallbackHelper();

        // Should be removed on navigation start.
        showAndWaitForInflation(startupPaintPreview, tabbedPaintPreview, dismissCallback);
        startupPaintPreview.getTabObserverForTesting().onRestoreStarted(tab);
        TestThreadUtils.runOnUiThreadBlocking(tab::reload);
        assertAttachedAndShown(tabbedPaintPreview, false, false);
        Assert.assertEquals(
                "Dismiss callback should have been called.", 1, dismissCallback.getCallCount());
    }

    private void assertSnackbarVisibility(SnackbarManager snackbarManager, boolean visible) {
        String message =
                visible ? "Snackbar should be visible." : "Snackbar should not be visible.";
        CriteriaHelper.pollUiThread(() -> snackbarManager.isShowing() == visible, message);
    }

    private void showAndWaitForInflation(StartupPaintPreview startupPaintPreview,
            TabbedPaintPreview tabbedPaintPreview, CallbackHelper dismissCallback) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> startupPaintPreview.show(
                                dismissCallback == null ? null : dismissCallback::notifyCalled));
        assertAttachedAndShown(tabbedPaintPreview, true, true);
        CriteriaHelper.pollUiThread(()
                                            -> tabbedPaintPreview.getViewForTesting() != null
                        && ((ViewGroup) tabbedPaintPreview.getViewForTesting()).getChildCount() > 0,
                "TabbedPaintPreview either doesn't have a view or a view with 0 children.");
    }

    private void simulateTap(View view, float x, float y)
            throws ExecutionException, InterruptedException {
        simulateTapWithDuration(view, x, y, 50);
    }

    private void simulateLongPress(View view, float x, float y)
            throws ExecutionException, InterruptedException {
        Thread.sleep(100);
        simulateTapWithDuration(view, x, y, ViewConfiguration.getLongPressTimeout() + 200);
    }

    private void simulateTapWithDuration(View view, float x, float y, long holdDurationMs)
            throws ExecutionException, InterruptedException {
        long downTime = SystemClock.uptimeMillis();
        MotionEvent down = MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, x, y, 0);
        TestThreadUtils.runOnUiThreadBlocking(() -> view.dispatchTouchEvent(down));

        Thread.sleep(holdDurationMs);
        MotionEvent up = MotionEvent.obtain(
                downTime, SystemClock.uptimeMillis(), MotionEvent.ACTION_UP, x, y, 0);
        TestThreadUtils.runOnUiThreadBlocking(() -> view.dispatchTouchEvent(up));
    }
}
