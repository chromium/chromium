// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.selection;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Matchers;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.Iterator;
import java.util.concurrent.TimeoutException;

/** Test that verifies back press will dismiss the selection popup. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
})
@Batch(Batch.PER_CLASS)
public class SelectionPopupBackPressTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_PAGE =
            UrlUtils.encodeHtmlDataUri(
                    "<html><head>  <meta name=viewport content='width=device-width,"
                        + " initial-scale=1.0'></head><body><p id=\"selection_popup_text\">Test</p>"
                        + "</body></html>");

    @Test
    @MediumTest
    @Feature({"TextInput", "SmartSelection"})
    @DisableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testBackPressClearSelection() throws TimeoutException {
        testBackPressClearSelectionInternal();
    }

    @Test
    @MediumTest
    @Feature({"TextInput", "SmartSelection"})
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testBackPressClearSelection_BackPressRefactor() throws TimeoutException {
        testBackPressClearSelectionInternal();
    }

    @Test
    @MediumTest
    @Feature({"TextInput", "SmartSelection"})
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testBackPressHandlerOnTabSwitched() {
        mActivityTestRule.startMainActivityOnBlankPage();
        final ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        final BackPressHandler selectionPopupHandler =
                activity.getBackPressManagerForTesting()
                        .getHandlersForTesting()[BackPressHandler.Type.SELECTION_POPUP];
        Assert.assertNotNull(
                "Back press handler should be initialized and registered.", selectionPopupHandler);
        Tab tab1 = activity.getActivityTab();
        var observers = ThreadUtils.runOnUiThreadBlocking(() -> TabTestUtils.getTabObservers(tab1));
        boolean found = find(observers, selectionPopupHandler);
        Assert.assertTrue("Tab should be observed.", found);

        mActivityTestRule.loadUrlInNewTab(TEST_PAGE);

        observers = ThreadUtils.runOnUiThreadBlocking(() -> TabTestUtils.getTabObservers(tab1));
        found = find(observers, selectionPopupHandler);
        Assert.assertFalse("Observer should be removed.", found);

        Tab currentTab = activity.getActivityTab();
        observers =
                ThreadUtils.runOnUiThreadBlocking(() -> TabTestUtils.getTabObservers(currentTab));
        found = find(observers, selectionPopupHandler);
        Assert.assertTrue("Tab should be observed.", found);
    }

    @Test
    @MediumTest
    @Feature({"TextInput", "SmartSelection"})
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testBackPressHandlerOnWebContentChanged() {
        mActivityTestRule.startMainActivityOnBlankPage();
        final ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        final SelectionPopupBackPressHandler selectionPopupHandler =
                (SelectionPopupBackPressHandler)
                        activity.getBackPressManagerForTesting()
                                .getHandlersForTesting()[BackPressHandler.Type.SELECTION_POPUP];
        Assert.assertNotNull(
                "Back press handler should be initialized and registered.", selectionPopupHandler);
        Tab tab1 = activity.getActivityTab();
        var observers = ThreadUtils.runOnUiThreadBlocking(() -> TabTestUtils.getTabObservers(tab1));
        boolean found = find(observers, selectionPopupHandler);
        Assert.assertTrue("Tab should be observed.", found);

        // Create a new tab such that back press handler is observing a new web content.
        mActivityTestRule.loadUrlInNewTab(TEST_PAGE);

        ThreadUtils.runOnUiThreadBlocking(() -> selectionPopupHandler.onContentChanged(tab1));

        var controller =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> SelectionPopupController.fromWebContents(tab1.getWebContents()));
        Assert.assertEquals(controller, selectionPopupHandler.getPopupControllerForTesting());
    }

    private void testBackPressClearSelectionInternal() throws TimeoutException {
        mActivityTestRule.startMainActivityWithURL(TEST_PAGE);
        DOMUtils.longPressNodeByJs(
                mActivityTestRule.getWebContents(),
                "document.getElementById('selection_popup_text')");
        SelectionPopupController controller =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return SelectionPopupController.fromWebContents(
                                    mActivityTestRule.getWebContents());
                        });
        Assert.assertNotNull(controller);
        // Wait until popup has been displayed.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Selection popup should be triggered after long press",
                            controller.isSelectActionBarShowing(),
                            Matchers.is(true));
                });
        Assert.assertTrue(
                "Selection popup should be triggered after long press.", controller.hasSelection());
        Assert.assertTrue(
                "Selection popup should be triggered after long press.",
                controller.isSelectActionBarShowingSupplier().get());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BackPressManager backPressManager =
                            mActivityTestRule.getActivity().getBackPressManagerForTesting();
                    if (backPressManager.has(BackPressHandler.Type.TEXT_BUBBLE)) {
                        mActivityTestRule
                                .getActivity()
                                .getBackPressManagerForTesting()
                                .removeHandler(BackPressHandler.Type.TEXT_BUBBLE);
                    }
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getActivity().getOnBackPressedDispatcher().onBackPressed();
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Selection popup should be dismissed after long press",
                            controller.isSelectActionBarShowing(),
                            Matchers.is(false));
                });
        Assert.assertFalse(
                "Selection popup should be dismissed on back press.", controller.hasSelection());
        Assert.assertFalse(
                "Selection popup should be dismissed on back press.",
                controller.isSelectActionBarShowingSupplier().get());
    }

    private boolean find(Iterator<TabObserver> observers, BackPressHandler handler) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    while (observers.hasNext()) {
                        if (observers.next() == handler) {
                            return true;
                        }
                    }
                    return false;
                });
    }
}
