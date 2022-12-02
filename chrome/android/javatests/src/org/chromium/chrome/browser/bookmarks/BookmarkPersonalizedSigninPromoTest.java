// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.core.AllOf.allOf;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.components.browser_ui.widget.RecyclerViewTestUtils.activeInRecyclerView;

import android.app.Activity;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SyncPromoController.SyncPromoState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.BookmarkTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * Tests for the personalized signin promo on the Bookmarks page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class BookmarkPersonalizedSigninPromoTest {
    private static final String CONTINUED_HISTOGRAM_NAME =
            "Signin.SyncPromo.Continued.Count.Bookmarks";
    private static final String SHOWN_HISTOGRAM_NAME = "Signin.SyncPromo.Shown.Count.Bookmarks";

    private final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private final BookmarkTestRule mBookmarkTestRule = new BookmarkTestRule();

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    // As bookmarks need the fake AccountManagerFacade in AccountManagerTestRule,
    // BookmarkTestRule should be initialized after and destroyed before the
    // AccountManagerTestRule.
    @Rule
    public final RuleChain chain =
            RuleChain.outerRule(mAccountManagerTestRule).around(mBookmarkTestRule);

    @Mock
    private SyncConsentActivityLauncher mMockSyncConsentActivityLauncher;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SIGNED_OUT_STATE);
        SyncConsentActivityLauncherImpl.setLauncherForTest(mMockSyncConsentActivityLauncher);
    }

    @After
    public void tearDown() {
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT);
        SyncConsentActivityLauncherImpl.setLauncherForTest(null);
        BookmarkPromoHeader.forcePromoStateForTests(null);
    }

    @Test
    @MediumTest
    public void testSigninButtonDefaultAccount() {
        final HistogramDelta continuedHistogram = new HistogramDelta(CONTINUED_HISTOGRAM_NAME, 1);
        final CoreAccountInfo accountInfo =
                mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        showBookmarkManagerAndCheckSigninPromoIsDisplayed();

        onView(allOf(withId(R.id.sync_promo_signin_button), activeInRecyclerView()))
                .perform(click());
        Assert.assertEquals(1, continuedHistogram.getDelta());
        Assert.assertEquals(
                mMockSyncConsentActivityLauncher, SyncConsentActivityLauncherImpl.get());
        verify(mMockSyncConsentActivityLauncher)
                .launchActivityForPromoDefaultFlow(any(Activity.class),
                        eq(SigninAccessPoint.BOOKMARK_MANAGER), eq(accountInfo.getEmail()));
    }

    @Test
    @MediumTest
    public void testSigninButtonNotDefaultAccount() {
        HistogramDelta continuedHistogram = new HistogramDelta(CONTINUED_HISTOGRAM_NAME, 1);
        final CoreAccountInfo accountInfo =
                mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        showBookmarkManagerAndCheckSigninPromoIsDisplayed();

        onView(allOf(withId(R.id.sync_promo_choose_account_button), activeInRecyclerView()))
                .perform(click());
        Assert.assertEquals(1, continuedHistogram.getDelta());
        Assert.assertEquals(
                mMockSyncConsentActivityLauncher, SyncConsentActivityLauncherImpl.get());
        verify(mMockSyncConsentActivityLauncher)
                .launchActivityForPromoChooseAccountFlow(any(Activity.class),
                        eq(SigninAccessPoint.BOOKMARK_MANAGER), eq(accountInfo.getEmail()));
    }

    @Test
    @MediumTest
    public void testSigninButtonNewAccount() {
        final HistogramDelta continuedHistogram = new HistogramDelta(CONTINUED_HISTOGRAM_NAME, 1);
        showBookmarkManagerAndCheckSigninPromoIsDisplayed();

        onView(allOf(withId(R.id.sync_promo_signin_button), activeInRecyclerView()))
                .perform(click());
        Assert.assertEquals(1, continuedHistogram.getDelta());
        Assert.assertEquals(
                mMockSyncConsentActivityLauncher, SyncConsentActivityLauncherImpl.get());
        verify(mMockSyncConsentActivityLauncher)
                .launchActivityForPromoAddAccountFlow(
                        any(Activity.class), eq(SigninAccessPoint.BOOKMARK_MANAGER));
    }

    private void showBookmarkManagerAndCheckSigninPromoIsDisplayed() {
        final HistogramDelta shownHistogram = new HistogramDelta(SHOWN_HISTOGRAM_NAME, 1);
        mBookmarkTestRule.showBookmarkManager(sActivityTestRule.getActivity());
        Assert.assertEquals(1, shownHistogram.getDelta());

        // TODO(https://cbug.com/1383638): If this stops the flakes, consider removing
        // activeInRecyclerView.
        RecyclerView recyclerView = mBookmarkTestRule.getBookmarkActivity().findViewById(
                R.id.selectable_list_recycler_view);
        Assert.assertNotNull(recyclerView);
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);

        // Profile data updates cause the signin promo to be recreated at the given index. The
        // RecyclerView's ViewGroup children may be stale, use activeInRecyclerView to filter to
        // only what is currently valid, otherwise the match will be ambiguous.
        onView(allOf(withId(R.id.signin_promo_view_container), activeInRecyclerView()))
                .check(matches(isDisplayed()));
    }
}
