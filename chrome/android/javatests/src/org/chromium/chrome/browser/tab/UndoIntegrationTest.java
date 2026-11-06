// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.tab_strip.TabStripFacility;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Tests undo and it's interactions with the UI. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class UndoIntegrationTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private static final String WINDOW_OPEN_BUTTON_URL =
            UrlUtils.encodeHtmlDataUri(
                    "<html>"
                            + "  <head>"
                            + "  <script>"
                            + "    function openWindow() {"
                            + "      window.open('about:blank');"
                            + "    }"
                            + "  </script>"
                            + "  </head>"
                            + "  <body>"
                            + "    <a id=\"link\" onclick=\"setTimeout(openWindow, 500);\">Open</a>"
                            + "  </body>"
                            + "</html>");

    @Before
    public void setUp() throws InterruptedException {
        SnackbarManager.setDurationForTesting(1500);
    }

    /** Test that a tab that is closing can't open other windows. */
    @Test
    @LargeTest
    @DisabledTest(message = "https://crbug.com/373950522")
    public void testAddNewContentsFromClosingTab() throws TimeoutException {
        // Load in a new tab as Chrome will close if the last tab is closed.
        mActivityTestRule.startOnWebPage(WINDOW_OPEN_BUTTON_URL);

        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        final Tab tab = TabModelUtils.getCurrentTab(model);

        // Click on the link that will trigger a delayed window popup. If this resolves it will open
        // a second about:blank tab.
        DOMUtils.clickNode(tab.getWebContents(), "link");

        // Attempt to close the tab, which will delay closing until the undo timeout goes away.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals("Model should have two tabs", 2, model.getCount());
                    model.getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(tab).allowUndo(true).build(),
                                    /* allowDialog= */ false);
                    Assert.assertTrue("Tab was not marked as closing", tab.isClosing());
                    Assert.assertTrue(
                            "Tab is not actually closing", model.isClosurePending(tab.getId()));
                });

        // Give the model a chance to process the pending closure.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(model.isClosurePending(tab.getId()), Matchers.is(false));
                    Criteria.checkThat(model.getCount(), Matchers.is(1));
                });

        // Validate that the model doesn't contain the original tab or any newly opened tabs.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(
                            "Model is still waiting to close the tab",
                            model.isClosurePending(tab.getId()));
                    Assert.assertEquals(
                            "Model has more than the expected about:blank tab",
                            1,
                            model.getCount());
                });
    }

    // Regression test for crbug.com/40276103.
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testTabletCloseTabAndCommitDoesNotCrash() {
        WebPageStation firstPage = mActivityTestRule.startOnBlankPage();
        int firstTabId = firstPage.getTab().getId();
        WebPageStation secondPage = firstPage.openFakeLinkToWebPage("about:blank");
        int secondTabId = secondPage.getTab().getId();

        // Wait for the strip to settle with both close buttons clickable.
        TabStripFacility<CtaPageStation> tabStrip = secondPage.expectTabStrip();
        CompositorButton firstCloseButton =
                tabStrip.expectTab(firstTabId).closeButtonElement.checkPresent();
        CompositorButton secondCloseButton =
                tabStrip.expectTab(secondTabId).closeButtonElement.checkPresent();
        StripLayoutHelperManager manager = tabStrip.stripLayoutHelperManagerElement.value();

        // Click both close buttons and commit in the same UI thread task, so that the commit runs
        // while both closes are still animating on the strip.
        TabModel model = secondPage.getTabModel();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    clickOnStrip(manager, secondCloseButton);
                    clickOnStrip(manager, firstCloseButton);
                    Assert.assertTrue(
                            "Both tabs should be closing",
                            model.isClosurePending(secondTabId)
                                    && model.isClosurePending(firstTabId));

                    model.commitAllTabClosures();
                });
    }

    /** Clicks the center of a strip view where it is drawn now. Run on the UI thread. */
    private static void clickOnStrip(StripLayoutHelperManager manager, StripLayoutView view) {
        manager.simulateClick(
                view.getDrawX() + view.getWidth() / 2,
                view.getDrawY() + view.getHeight() / 2,
                /* buttons= */ 0,
                /* modifiers= */ 0);
    }
}
