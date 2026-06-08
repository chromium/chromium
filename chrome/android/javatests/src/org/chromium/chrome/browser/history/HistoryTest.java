// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.transit.ViewFinder.expectNoView;
import static org.chromium.base.test.transit.ViewFinder.expectView;
import static org.chromium.base.test.transit.ViewFinder.waitForNoView;
import static org.chromium.base.test.transit.ViewFinder.waitForView;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalHistoryUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeHistoryUrl;

import android.graphics.Bitmap;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Tests for history feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class HistoryTest {

    private static class FaviconWaiter extends CallbackHelper
            implements FaviconHelper.FaviconImageCallback {
        private Bitmap mFavicon;

        @Override
        public void onFaviconAvailable(Bitmap image, GURL iconUrl) {
            mFavicon = image;
            notifyCalled();
        }

        public Bitmap waitForFavicon() throws TimeoutException {
            waitForOnly();
            return mFavicon;
        }
    }

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public SigninTestRule mSigninTestRule = new SigninTestRule();

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getForProfile(ProfileManager.getLastUsedRegularProfile())
                            .clearBrowsingData(
                                    () -> {},
                                    new int[] {BrowsingDataType.HISTORY},
                                    TimePeriod.ALL_TIME);
                });
    }

    /**
     * Check that the favicons for {@link getOriginalHistoryUrl()} and for {@link
     * getOriginalNativeHistoryUrl()} are identical.
     */
    @Test
    @SmallTest
    public void testFavicon() throws Exception {
        mActivityTestRule.startOnBlankPage();

        FaviconHelper helper = ThreadUtils.runOnUiThreadBlocking(FaviconHelper::new);

        Bitmap nonNativeFavicon = getFavicon(helper, new GURL(getOriginalHistoryUrl()));
        Bitmap nativeFavicon = getFavicon(helper, new GURL(getOriginalNativeHistoryUrl()));

        assertNotNull(nonNativeFavicon);
        assertNotNull(nativeFavicon);
        assertTrue(nonNativeFavicon.sameAs(nativeFavicon));
    }

    public Bitmap getFavicon(FaviconHelper helper, GURL pageUrl) throws TimeoutException {
        FaviconWaiter waiter = new FaviconWaiter();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    helper.getLocalFaviconImageForURL(
                            ProfileManager.getLastUsedRegularProfile(),
                            pageUrl,
                            0,
                            /* fallbackToHost= */ true,
                            waiter);
                });
        return waiter.waitForFavicon();
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Flaky test, see crbug.com/441282177")
    // Tests that the history sync opt-in promo is shown correctly when display conditions are met,
    // and the history sync opt-in flow works correctly when the CTA is clicked.
    public void testHistorySyncPromoHeader_withHistoryRecord() throws Exception {
        // Sign-in with an account and opt-out history sync.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SyncService syncService = SyncTestUtil.getSyncServiceForLastUsedProfile();
                    syncService.setSelectedType(UserSelectableType.HISTORY, false);
                    syncService.setSelectedType(UserSelectableType.TABS, false);
                });
        mActivityTestRule.startOnBlankPage();
        // Load a page so the history page will not in empty state.
        String testUrl = "/chrome/test/data/android/google.html";
        mActivityTestRule.loadUrl(mActivityTestRule.getTestServer().getURL(testUrl));

        mActivityTestRule.loadUrlInNewTab(getOriginalHistoryUrl());

        // Verify that the promo is shown.
        waitForView(withId(R.id.signin_promo_view_container));
        // Click on the promo CTA.
        expectView(withId(R.id.sync_promo_signin_button)).click();

        // Verify that the history sync screen is shown.
        waitForView(withId(R.id.history_sync_illustration));

        // Opt-in history sync and finish the opt-in activity.
        waitForView(withId(R.id.button_primary)).click();
        SyncTestUtil.waitForHistorySyncEnabled();

        // Verify that the content is still shown and the promo is dismissed.
        waitForNoView(withId(R.id.signin_promo_view_container));
        expectView(withId(R.id.history_page_recycler_view));
        expectNoView(withId(R.id.empty_state_container));
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @DisabledTest(message = "Flaky test, see crbug.com/441282177")
    // Tests that the history sync opt-in promo when there's no history record, to verify
    // interactions with the history page empty state.
    //
    // Flaky on tablets in landscape and consistently failing on auto in landscape.
    // See crbug.com/431136352.
    public void testHistorySyncPromoHeader_noHistoryRecord() throws Exception {
        // Sign-in with an account and opt-out history sync.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SyncService syncService = SyncTestUtil.getSyncServiceForLastUsedProfile();
                    syncService.setSelectedType(UserSelectableType.HISTORY, false);
                    syncService.setSelectedType(UserSelectableType.TABS, false);
                });
        mActivityTestRule.startOnBlankPage();

        mActivityTestRule.loadUrlInNewTab(getOriginalHistoryUrl());

        // Verify that the promo is shown.
        waitForView(withId(R.id.signin_promo_view_container));
        // Click on the promo CTA.
        expectView(withId(R.id.sync_promo_signin_button)).click();

        // Verify that the history sync screen is shown.
        waitForView(withId(R.id.history_sync_illustration));

        // Opt-in history sync and finish the opt-in activity.
        waitForView(withId(R.id.button_primary)).click();
        SyncTestUtil.waitForHistorySyncEnabled();

        // Verify that the empty state is shown when the promo is dismissed.
        waitForView(withId(R.id.empty_state_container));
        expectNoView(withId(R.id.history_page_recycler_view));
    }

    @MediumTest
    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.DRAW_CHROME_PAGES_EDGE_TO_EDGE,
        ChromeFeatureList.EDGE_TO_EDGE_MONITOR_CONFIGURATIONS
    })
    @Restriction({DeviceFormFactor.PHONE, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testDrawsEdgeToEdge() {
        mActivityTestRule.startOnBlankPage();
        HistoryActivity historyActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        HistoryActivity.class,
                        new MenuUtils.MenuActivityTrigger(
                                InstrumentationRegistry.getInstrumentation(),
                                mActivityTestRule.getActivity(),
                                R.id.open_history_menu_id));
        assertNotNull(historyActivity);
        RecyclerView recyclerView =
                historyActivity
                        .getHistoryManagerForTests()
                        .getSelectableListLayout()
                        .getRecyclerViewForTesting();
        assertNotNull(recyclerView);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    EdgeToEdgeController edgeToEdgeController =
                            historyActivity.getEdgeToEdgeSupplier().get();
                    int bottomInset =
                            edgeToEdgeController != null && edgeToEdgeController.isDrawingToEdge()
                                    ? edgeToEdgeController.getBottomInsetPx()
                                    : 0;
                    assertEquals(bottomInset, recyclerView.getPaddingBottom());
                    if (bottomInset > 0) {
                        // Clip to padding should be false when padding for the bottom inset.
                        assertFalse(recyclerView.getClipToPadding());
                    } else {
                        // Clip to padding should be true when there is no padding for the bottom
                        // inset.
                        assertTrue(recyclerView.getClipToPadding());
                    }
                });

        historyActivity.finish();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_HISTORY_CLUSTERING)
    public void testHistoryClustering_ExpandCollapse() throws Exception {
        mActivityTestRule.startOnBlankPage();
        String urlOne =
                mActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/navigate/one.html");
        String urlTwo =
                mActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/navigate/two.html");
        String domain = new GURL(urlOne).getHost();

        mActivityTestRule.loadUrl(urlOne);
        mActivityTestRule.loadUrl(urlTwo);

        mActivityTestRule.loadUrlInNewTab(getOriginalHistoryUrl());

        waitForView(withId(R.id.history_page_recycler_view));
        KeyboardUtils.hideAndroidSoftKeyboard(
                mActivityTestRule.getActivity().getWindow().getDecorView());

        // Initial state: cluster is collapsed. "One" and "Two" should not be displayed.
        waitForView(withText(domain));

        expectNoView(withText("One"));
        expectNoView(withText("Two"));

        // Click on the expand button. It has the content description "Expand"
        expectView(
                        org.hamcrest.Matchers.allOf(
                                withContentDescription(
                                        mActivityTestRule
                                                .getActivity()
                                                .getString(R.string.accessibility_expand_section)),
                                isDisplayed()))
                .click();

        // Now "One" and "Two" should be displayed.
        waitForView(withText("One"));
        waitForView(withText("Two"));

        // Click on the collapse button.
        expectView(
                        org.hamcrest.Matchers.allOf(
                                withContentDescription(
                                        mActivityTestRule
                                                .getActivity()
                                                .getString(
                                                        R.string.accessibility_collapse_section)),
                                isDisplayed()))
                .click();

        // "One" and "Two" should be hidden.
        waitForNoView(withText("One"));
        waitForNoView(withText("Two"));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_HISTORY_CLUSTERING)
    // Flaky: crbug.com/493318914
    @DisableIf.Device(DeviceFormFactor.DESKTOP)
    public void testHistoryClustering_RemoveItem() throws Exception {
        mActivityTestRule.startOnBlankPage();
        String urlOne =
                mActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/navigate/one.html");
        String urlTwo =
                mActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/navigate/two.html");
        String domain = new GURL(urlOne).getHost();

        mActivityTestRule.loadUrl(urlOne);
        mActivityTestRule.loadUrl(urlTwo);

        mActivityTestRule.loadUrlInNewTab(getOriginalHistoryUrl());

        waitForView(withId(R.id.history_page_recycler_view));
        KeyboardUtils.hideAndroidSoftKeyboard(
                mActivityTestRule.getActivity().getWindow().getDecorView());

        // Expand the cluster.
        waitForView(
                        org.hamcrest.Matchers.allOf(
                                withContentDescription(
                                        mActivityTestRule
                                                .getActivity()
                                                .getString(R.string.accessibility_expand_section)),
                                isDisplayed()))
                .click();

        // Wait for items to be visible.
        waitForView(withText("One"));

        // Remove "One".
        expectView(
                        org.hamcrest.Matchers.allOf(
                                withId(R.id.end_button),
                                withContentDescription(R.string.remove),
                                isDescendantOfA(
                                        org.hamcrest.Matchers.allOf(
                                                hasDescendant(withText("One")),
                                                isAssignableFrom(HistoryItemView.class)))))
                .click();

        // "One" should disappear.
        waitForNoView(withText("One"));

        // The cluster should dissolve, so "Two" is visible as a normal item.
        waitForView(withText("Two"));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_HISTORY_CLUSTERING)
    @DisableIf.Device(DeviceFormFactor.DESKTOP) // Flaky on desktop crbug.com/498132516
    public void testHistoryClustering_RemoveCluster() throws Exception {
        mActivityTestRule.startOnBlankPage();
        String urlOne =
                mActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/navigate/one.html");
        String urlTwo =
                mActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/navigate/two.html");
        String domain = new GURL(urlOne).getHost();

        mActivityTestRule.loadUrl(urlOne);
        mActivityTestRule.loadUrl(urlTwo);

        mActivityTestRule.loadUrlInNewTab(getOriginalHistoryUrl());

        waitForView(withId(R.id.history_page_recycler_view));
        KeyboardUtils.hideAndroidSoftKeyboard(
                mActivityTestRule.getActivity().getWindow().getDecorView());

        // Verify the cluster is created and select it.
        waitForView(withText(domain)).longPress();

        // Click delete on the toolbar.
        waitForView(withId(R.id.selection_mode_delete_menu_id)).click();

        // Ensure "domain" is removed.
        waitForNoView(withText(domain));

        // Also ensure the items are actually removed (they shouldn't be visible).
        waitForNoView(withText("One"));
        waitForNoView(withText("Two"));
    }
}
