// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.graphics.Color;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.feed.v2.FeedV2TestHelper;
import org.chromium.chrome.browser.feed.v2.TestFeedServer;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests for colors used in UI components in the native android New Tab Page. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "disable-features=IPH_FeedHeaderMenu"
})
public class NewTabPageColorWithFeedV2Test {
    private static final int MIN_ITEMS_AFTER_LOAD = 10;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Tab mTab;
    private NewTabPage mNtp;

    private TestFeedServer mFeedServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityWithURL("about:blank");

        // Allow rendering external items without the external renderer.

        // EULA must be accepted, and internet connectivity is required, or the Feed will not
        // attempt to load.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    NetworkChangeNotifier.forceConnectivityState(true);
                    FirstRunUtils.setEulaAccepted();
                });

        mFeedServer = new TestFeedServer();

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testTextBoxBackgroundColor() throws Exception {
        RecyclerView recycleView = mNtp.getCoordinatorForTesting().getRecyclerView();

        Context context = mActivityTestRule.getActivity();
        int expectedTextBoxBackground =
                ChromeColors.getSurfaceColor(
                        context, R.dimen.home_surface_background_color_elevation);
        Assert.assertEquals(
                expectedTextBoxBackground, mNtp.getToolbarTextBoxBackgroundColor(Color.BLACK));

        // Wait for the test feed items to be available in the feed.
        FeedV2TestHelper.waitForRecyclerItems(
                MIN_ITEMS_AFTER_LOAD,
                (RecyclerView) mNtp.getCoordinatorForTesting().getRecyclerView());

        // Scroll to the bottom.
        RecyclerViewTestUtils.scrollToBottom(recycleView);
        RecyclerViewTestUtils.waitForStableRecyclerView(recycleView);

        Assert.assertTrue(mNtp.isLocationBarScrolledToTopInNtp());
        expectedTextBoxBackground = SemanticColorUtils.getColorPrimaryContainer(context);
        Assert.assertEquals(
                expectedTextBoxBackground, mNtp.getToolbarTextBoxBackgroundColor(Color.BLACK));
    }
}
