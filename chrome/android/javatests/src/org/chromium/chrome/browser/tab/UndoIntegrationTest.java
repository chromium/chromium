// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Tests undo and it's interactions with the UI.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class UndoIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String WINDOW_OPEN_BUTTON_URL = UrlUtils.encodeHtmlDataUri(
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
            + "</html>"
    );

    @Before
    public void setUp() throws InterruptedException {
        SnackbarManager.setDurationForTesting(1500);
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Test that a tab that is closing can't open other windows.
     * @throws TimeoutException
     */
    @Test
    @DisabledTest(message = "https://crbug.com/679480")
    @LargeTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testAddNewContentsFromClosingTab() throws TimeoutException {
        mActivityTestRule.loadUrl(WINDOW_OPEN_BUTTON_URL);

        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        final Tab tab = TabModelUtils.getCurrentTab(model);

        // Clock on the link that will trigger a delayed window popup.
        DOMUtils.clickNode(tab.getWebContents(), "link");

        // Attempt to close the tab, which will delay closing until the undo timeout goes away.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabModelUtils.closeTabById(model, tab.getId(), true);
            Assert.assertTrue("Tab was not marked as closing", tab.isClosing());
            Assert.assertTrue("Tab is not actually closing", model.isClosurePending(tab.getId()));
        });

        // Give the model a chance to process the undo and close the tab.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(model.isClosurePending(tab.getId()), Matchers.is(false));
            Criteria.checkThat(model.getCount(), Matchers.is(0));
        });

        // Validate that the model doesn't contain the original tab or any newly opened tabs.
        Assert.assertFalse(
                "Model is still waiting to close the tab", model.isClosurePending(tab.getId()));
        Assert.assertEquals("Model still has tabs", 0, model.getCount());
    }
}
