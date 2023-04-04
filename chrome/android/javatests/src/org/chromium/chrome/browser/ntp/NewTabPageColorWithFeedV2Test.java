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

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.feed.v2.FeedV2TestHelper;
import org.chromium.chrome.browser.feed.v2.TestFeedServer;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests for colors used in UI components in the native android New Tab Page.
 */
// clang-format off
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
                       "disable-features=IPH_FeedHeaderMenu"})
@Features.DisableFeatures({ChromeFeatureList.QUERY_TILES, ChromeFeatureList.VIDEO_TUTORIALS})
// clang-format on
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
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

    // clang-format off
    @Test
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testTextBoxBackgroundColor() throws Exception {
        // clang-format on
        RecyclerView recycleView = (RecyclerView) mNtp.getCoordinatorForTesting().getRecyclerView();

        Context context = mActivityTestRule.getActivity();
        Assert.assertEquals(ChromeColors.getPrimaryBackgroundColor(context, false),
                mNtp.getToolbarTextBoxBackgroundColor(Color.BLACK));

        // Wait for the test feed items to be available in the feed.
        FeedV2TestHelper.waitForRecyclerItems(MIN_ITEMS_AFTER_LOAD,
                (RecyclerView) mNtp.getCoordinatorForTesting().getRecyclerView());

        // Scroll to the bottom.
        RecyclerViewTestUtils.scrollToBottom(recycleView);
        RecyclerViewTestUtils.waitForStableRecyclerView(recycleView);

        Assert.assertTrue(mNtp.isLocationBarScrolledToTopInNtp());
        final int expectedTextBoxBackground =
                ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_2);
        Assert.assertEquals(
                expectedTextBoxBackground, mNtp.getToolbarTextBoxBackgroundColor(Color.BLACK));
    }
}
