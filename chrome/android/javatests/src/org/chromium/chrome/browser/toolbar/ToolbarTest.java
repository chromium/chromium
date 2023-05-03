// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.findinpage.FindToolbar;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests for toolbar manager behavior.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ToolbarTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_PAGE = "/chrome/test/data/android/test.html";

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private void findInPageFromMenu() {
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), R.id.find_in_page_id);

        waitForFindInPageVisibility(true);
    }

    private void waitForFindInPageVisibility(final boolean visible) {
        CriteriaHelper.pollUiThread(() -> {
            FindToolbar findToolbar =
                    (FindToolbar) mActivityTestRule.getActivity().findViewById(R.id.find_toolbar);
            if (visible) {
                Criteria.checkThat(findToolbar, Matchers.notNullValue());
                Criteria.checkThat(findToolbar.isShown(), Matchers.is(true));
            } else {
                if (findToolbar == null) return;
                Criteria.checkThat(findToolbar.isShown(), Matchers.is(false));
            }
            Criteria.checkThat(findToolbar.isAnimating(), Matchers.is(false));
        });
    }

    private boolean isErrorPage(final Tab tab) {
        final boolean[] isShowingError = new boolean[1];
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { isShowingError[0] = tab.isShowingErrorPage(); });
        return isShowingError[0];
    }

    @Test
    @MediumTest
    public void testOmniboxScrim() {
        ChromeActivity activity = mActivityTestRule.getActivity();
        ToolbarManager toolbarManager = activity.getToolbarManager();
        ScrimCoordinator scrimCoordinator =
                activity.getRootUiCoordinatorForTesting().getScrimCoordinatorForTesting();
        scrimCoordinator.disableAnimationForTesting(true);

        assertNull("The scrim should be null.", scrimCoordinator.getViewForTesting());
        assertFalse("All tabs should not currently be obscured.",
                activity.getTabObscuringHandler().isTabContentObscured());

        ThreadUtils.runOnUiThreadBlocking(() -> toolbarManager.setUrlBarFocus(true, 0));

        assertNotNull("The scrim should not be null.", scrimCoordinator.getViewForTesting());
        assertTrue("All tabs should currently be obscured.",
                activity.getTabObscuringHandler().isTabContentObscured());

        ThreadUtils.runOnUiThreadBlocking(() -> toolbarManager.setUrlBarFocus(false, 0));

        assertNull("The scrim should be null.", scrimCoordinator.getViewForTesting());
        assertFalse("All tabs should not currently be obscured.",
                activity.getTabObscuringHandler().isTabContentObscured());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1230091")
    public void testNTPNavigatesToErrorPageOnDisconnectedNetwork() {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartServer(
                ApplicationProvider.getApplicationContext());
        String testUrl = testServer.getURL(TEST_PAGE);

        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        // Load new tab page.
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        Assert.assertEquals(UrlConstants.NTP_URL, ChromeTabUtils.getUrlStringOnUiThread(tab));
        assertFalse(isErrorPage(tab));

        // Stop the server and also disconnect the network.
        testServer.stopAndDestroyServer();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> NetworkChangeNotifier.forceConnectivityState(false));

        mActivityTestRule.loadUrl(testUrl);
        Assert.assertEquals(testUrl, ChromeTabUtils.getUrlStringOnUiThread(tab));
        assertTrue(isErrorPage(tab));
    }

    @Test
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"Omnibox"})
    public void testFindInPageDismissedOnOmniboxFocus() {
        findInPageFromMenu();
        OmniboxTestUtils omnibox = new OmniboxTestUtils(mActivityTestRule.getActivity());
        omnibox.requestFocus();
        waitForFindInPageVisibility(false);
    }
}
