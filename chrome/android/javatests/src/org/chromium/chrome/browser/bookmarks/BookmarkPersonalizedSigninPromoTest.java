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
import static org.mockito.Mockito.when;

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
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SyncPromoController.SyncPromoState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.BookmarkTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncFeatureMap;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.Set;

/** Tests for the personalized signin promo on the Bookmarks page. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@DisableFeatures({
    SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE,
    // TODO(crbug.com/344981899): ReplaceSyncPromosWithSigninPromos is disabled because bookmarks
    // account storage is disabled above, otherwise tests run into assertion failures. Long term,
    // these tests probably need to be fixed for the bookmarks account storage case rather than
    // force-disable the feature.
    ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS,
    ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP
})
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

    @Mock private SyncConsentActivityLauncher mMockSyncConsentActivityLauncher;

    @Mock private SyncService mSyncService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        SyncConsentActivityLauncherImpl.setLauncherForTest(mMockSyncConsentActivityLauncher);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT);
        BookmarkPromoHeader.forcePromoStateForTesting(null);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1406333")
    public void testSigninButtonDefaultAccount() {
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.PROMO_FOR_SIGNED_OUT_STATE);

        var continuedHistogram =
                HistogramWatcher.newSingleRecordWatcher(CONTINUED_HISTOGRAM_NAME, 1);
        final CoreAccountInfo accountInfo =
                mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        showBookmarkManagerAndCheckSigninPromoIsDisplayed();

        onView(allOf(withId(R.id.sync_promo_signin_button), activeInRecyclerView()))
                .perform(click());
        continuedHistogram.assertExpected();
        Assert.assertEquals(
                mMockSyncConsentActivityLauncher, SyncConsentActivityLauncherImpl.get());
        verify(mMockSyncConsentActivityLauncher)
                .launchActivityForPromoDefaultFlow(
                        any(Activity.class),
                        eq(SigninAccessPoint.BOOKMARK_MANAGER),
                        eq(accountInfo.getEmail()));
    }

    @Test
    @MediumTest
    // Signing in with a non-default account is disabled on automotive, which only supports one
    // account per OS profile.
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    @DisableIf.Device(DeviceFormFactor.TABLET) // crbug.com/362215887
    public void testSigninButtonNotDefaultAccount() {
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.PROMO_FOR_SIGNED_OUT_STATE);

        var continuedHistogram =
                HistogramWatcher.newSingleRecordWatcher(CONTINUED_HISTOGRAM_NAME, 1);
        final CoreAccountInfo accountInfo =
                mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        showBookmarkManagerAndCheckSigninPromoIsDisplayed();

        onView(allOf(withId(R.id.sync_promo_choose_account_button), activeInRecyclerView()))
                .perform(click());
        continuedHistogram.assertExpected();
        Assert.assertEquals(
                mMockSyncConsentActivityLauncher, SyncConsentActivityLauncherImpl.get());
        verify(mMockSyncConsentActivityLauncher)
                .launchActivityForPromoChooseAccountFlow(
                        any(Activity.class),
                        eq(SigninAccessPoint.BOOKMARK_MANAGER),
                        eq(accountInfo.getEmail()));
    }

    @Test
    @MediumTest
    @DisableIf.Device(DeviceFormFactor.TABLET) // crbug.com/362215887
    public void testSigninButtonNewAccount() {
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.PROMO_FOR_SIGNED_OUT_STATE);

        var continuedHistogram =
                HistogramWatcher.newSingleRecordWatcher(CONTINUED_HISTOGRAM_NAME, 1);
        showBookmarkManagerAndCheckSigninPromoIsDisplayed();

        onView(allOf(withId(R.id.sync_promo_signin_button), activeInRecyclerView()))
                .perform(click());
        continuedHistogram.assertExpected();
        Assert.assertEquals(
                mMockSyncConsentActivityLauncher, SyncConsentActivityLauncherImpl.get());
        verify(mMockSyncConsentActivityLauncher)
                .launchActivityForPromoAddAccountFlow(
                        any(Activity.class), eq(SigninAccessPoint.BOOKMARK_MANAGER));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS,
        ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP
    })
    // TODO(crbug.com/353949455): Re-enable these tests once UNO is enabled for cars.
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void shouldHideBookmarksSigninPromoIfBookmarksIsManagedByPolicy() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS)).thenReturn(true);

        showBookmarkManagerAndCheckSigninPromoIsHidden();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS,
        ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP
    })
    // TODO(crbug.com/353949455): Re-enable these tests once UNO is enabled for cars.
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void shouldShowBookmarksSigninPromoIfBookmarksIsNotManagedByPolicy() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS)).thenReturn(false);

        showBookmarkManagerAndCheckSigninPromoIsDisplayed(/* checkHistogram= */ false);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS,
        ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP
    })
    // TODO(crbug.com/353949455): Re-enable these tests once UNO is enabled for cars.
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void shouldHideBookmarksSigninPromoIfDataTypesSyncing() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS)).thenReturn(false);
        when(mSyncService.getSelectedTypes())
                .thenReturn(Set.of(UserSelectableType.BOOKMARKS, UserSelectableType.READING_LIST));

        showBookmarkManagerAndCheckSigninPromoIsHidden();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS,
        ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP
    })
    // TODO(crbug.com/353949455): Re-enable these tests once UNO is enabled for cars.
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void shouldShowBookmarksSigninPromoIfBookmarkNotSyncing() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS)).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.READING_LIST));

        showBookmarkManagerAndCheckSigninPromoIsDisplayed(/* checkHistogram= */ false);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS,
        ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP
    })
    // TODO(crbug.com/353949455): Re-enable these tests once UNO is enabled for cars.
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void shouldShowBookmarksSigninPromoIfReadingListNotSyncing() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS)).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.BOOKMARKS));

        showBookmarkManagerAndCheckSigninPromoIsDisplayed(/* checkHistogram= */ false);
    }

    // Get the activity that hosts the bookmark UI - on phones, this is a BookmarkActivity, on
    // tablets this is a native page.
    private Activity getBookmarkHostActivity() {
        if (sActivityTestRule.getActivity().isTablet()) {
            return sActivityTestRule.getActivity();
        } else {
            return mBookmarkTestRule.getBookmarkActivity();
        }
    }

    private void showBookmarkManagerAndCheckSigninPromoIsDisplayed() {
        showBookmarkManagerAndCheckSigninPromoIsDisplayed(true);
    }

    // TODO(crbug.com/327387704): Once we implement the correct impression recording, always check
    // histograms.
    private void showBookmarkManagerAndCheckSigninPromoIsDisplayed(boolean checkHistogram) {
        var shownHistogram = HistogramWatcher.newSingleRecordWatcher(SHOWN_HISTOGRAM_NAME, 1);
        mBookmarkTestRule.showBookmarkManager(sActivityTestRule.getActivity());
        if (checkHistogram) {
            shownHistogram.assertExpected();
        }

        // TODO(https://cbug.com/1383638): If this stops the flakes, consider removing
        // activeInRecyclerView.
        RecyclerView recyclerView =
                getBookmarkHostActivity().findViewById(R.id.selectable_list_recycler_view);
        Assert.assertNotNull(recyclerView);
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);

        // Profile data updates cause the signin promo to be recreated at the given index. The
        // RecyclerView's ViewGroup children may be stale, use activeInRecyclerView to filter to
        // only what is currently valid, otherwise the match will be ambiguous.
        onView(allOf(withId(R.id.signin_promo_view_container), activeInRecyclerView()))
                .check(matches(isDisplayed()));
    }

    private void showBookmarkManagerAndCheckSigninPromoIsHidden() {
        mBookmarkTestRule.showBookmarkManager(sActivityTestRule.getActivity());

        RecyclerView recyclerView =
                getBookmarkHostActivity().findViewById(R.id.selectable_list_recycler_view);
        Assert.assertNotNull(recyclerView);
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);

        Assert.assertNull(
                mBookmarkTestRule
                        .getBookmarkActivity()
                        .findViewById(R.id.signin_promo_view_container));
    }
}
