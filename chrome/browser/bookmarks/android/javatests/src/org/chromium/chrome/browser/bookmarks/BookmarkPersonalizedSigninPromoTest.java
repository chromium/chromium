// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.core.AllOf.allOf;
import static org.mockito.Mockito.when;

import static org.chromium.components.browser_ui.widget.RecyclerViewTestUtils.activeInRecyclerView;

import android.app.Activity;
import android.os.Build;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.BookmarkTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Set;

/** Tests for the personalized signin promo on the Bookmarks page. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)
public class BookmarkPersonalizedSigninPromoTest {
    private static final String CONTINUED_HISTOGRAM_NAME =
            "Signin.SyncPromo.Continued.Count.Bookmarks";
    private static final String SHOWN_HISTOGRAM_NAME = "Signin.SyncPromo.Shown.Count.Bookmarks";

    private final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private final BookmarkTestRule mBookmarkTestRule = new BookmarkTestRule();

    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    // As bookmarks need the fake AccountManagerFacade in AccountManagerTestRule,
    // BookmarkTestRule should be initialized after and destroyed before the
    // AccountManagerTestRule.
    @Rule
    public final RuleChain chain =
            RuleChain.outerRule(mAccountManagerTestRule).around(mBookmarkTestRule);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private SyncService mSyncService;

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED);
        ChromeSharedPreferences.getInstance()
                .removeKey(
                        ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                                SigninPreferencesManager.SigninPromoAccessPointId.BOOKMARKS));
    }

    @Test
    @MediumTest
    @DisableIf.Device(DeviceFormFactor.TABLET) // crbug.com/372858049
    public void shouldHideBookmarksSigninPromoIfBookmarksIsManagedByPolicy() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS)).thenReturn(true);

        showBookmarkManagerAndCheckSigninPromoIsHidden();
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.S_V2, message = "crbug.com/362215887")
    public void shouldShowBookmarksSigninPromoIfBookmarksIsNotManagedByPolicy() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS)).thenReturn(false);

        showBookmarkManagerAndCheckSigninPromoIsDisplayed();
    }

    @Test
    @MediumTest
    @DisableIf.Device(DeviceFormFactor.TABLET) // crbug.com/372858049
    public void shouldHideBookmarksSigninPromoIfDataTypesSyncing() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS)).thenReturn(false);
        when(mSyncService.getSelectedTypes())
                .thenReturn(Set.of(UserSelectableType.BOOKMARKS, UserSelectableType.READING_LIST));

        showBookmarkManagerAndCheckSigninPromoIsHidden();
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.S_V2, message = "crbug.com/362215887")
    public void shouldShowBookmarksSigninPromoIfBookmarkNotSyncing() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS)).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.READING_LIST));

        showBookmarkManagerAndCheckSigninPromoIsDisplayed();
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.S_V2, message = "crbug.com/362215887")
    public void shouldShowBookmarksSigninPromoIfReadingListNotSyncing() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS)).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.BOOKMARKS));

        showBookmarkManagerAndCheckSigninPromoIsDisplayed();
    }

    // Get the activity that hosts the bookmark UI - on phones, this is a BookmarkActivity, on
    // tablets this is a native page.
    private Activity getBookmarkHostActivity() {
        if (mActivityTestRule.getActivity().isTablet()) {
            return mActivityTestRule.getActivity();
        } else {
            return mBookmarkTestRule.getBookmarkActivity();
        }
    }

    private void showBookmarkManagerAndCheckSigninPromoIsDisplayed() {
        var shownHistogram = HistogramWatcher.newSingleRecordWatcher(SHOWN_HISTOGRAM_NAME, 1);
        mBookmarkTestRule.showBookmarkManager(mActivityTestRule.getActivity());
        shownHistogram.assertExpected();

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
        mBookmarkTestRule.showBookmarkManager(mActivityTestRule.getActivity());

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
