// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.ui.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;
import static org.chromium.ui.test.util.ViewUtils.waitForViewCheckingState;

import android.graphics.Bitmap;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.embedder_support.util.UrlConstants;
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

    /**
     * Check that the favicons for {@link UrlConstants#HISTORY_URL} and for {@link
     * UrlConstants#NATIVE_HISTORY_URL} are identical.
     */
    @Test
    @SmallTest
    public void testFavicon() throws Exception {
        mActivityTestRule.startOnBlankPage();

        FaviconHelper helper = ThreadUtils.runOnUiThreadBlocking(FaviconHelper::new);
        // If the returned favicons are non-null Bitmap#sameAs() should be used.
        assertNull(getFavicon(helper, new GURL(UrlConstants.HISTORY_URL)));
        assertNull(getFavicon(helper, new GURL(UrlConstants.NATIVE_HISTORY_URL)));
    }

    public Bitmap getFavicon(FaviconHelper helper, GURL pageUrl) throws TimeoutException {
        FaviconWaiter waiter = new FaviconWaiter();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    helper.getLocalFaviconImageForURL(
                            ProfileManager.getLastUsedRegularProfile(), pageUrl, 0, waiter);
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

        mActivityTestRule.loadUrlInNewTab(UrlConstants.HISTORY_URL);

        // Verify that the promo is shown.
        onViewWaiting(withId(R.id.signin_promo_view_container));
        // Click on the promo CTA.
        onView(withId(R.id.sync_promo_signin_button)).perform(click());

        // Verify that the history sync screen is shown.
        onViewWaiting(withId(R.id.history_sync_illustration), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));

        // Opt-in history sync and finish the opt-in activity.
        onViewWaiting(withId(R.id.button_primary)).perform(click());
        SyncTestUtil.waitForHistorySyncEnabled();

        // Verify that the content is still shown and the promo is dismissed.
        waitForViewCheckingState(withId(R.id.signin_promo_view_container), VIEW_NULL);
        onView(withId(R.id.history_page_recycler_view)).check(matches(isDisplayed()));
        onView(withId(R.id.empty_state_container)).check(matches(not(isDisplayed())));
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

        mActivityTestRule.loadUrlInNewTab(UrlConstants.HISTORY_URL);

        // Verify that the promo is shown.
        onViewWaiting(withId(R.id.signin_promo_view_container));
        // Click on the promo CTA.
        onView(withId(R.id.sync_promo_signin_button)).perform(click());

        // Verify that the history sync screen is shown.
        onViewWaiting(withId(R.id.history_sync_illustration), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));

        // Opt-in history sync and finish the opt-in activity.
        onViewWaiting(withId(R.id.button_primary)).perform(click());
        SyncTestUtil.waitForHistorySyncEnabled();

        // Verify that the empty state is shown when the promo is dismissed.
        onViewWaiting(withId(R.id.empty_state_container)).check(matches(isDisplayed()));
        onView(withId(R.id.history_page_recycler_view)).check(matches(not(isDisplayed())));
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
}
