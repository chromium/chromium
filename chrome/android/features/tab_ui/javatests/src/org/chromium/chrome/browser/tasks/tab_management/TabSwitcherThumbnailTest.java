// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.contrib.RecyclerViewActions.scrollToPosition;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;
import android.widget.ImageView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.ViewAssertion;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

/** Tests for the thumbnail view in Grid Tab Switcher. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group"
})
@Restriction({DeviceFormFactor.PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
public class TabSwitcherThumbnailTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ThumbnailFetcher mNullThumbnailFetcher =
            new ThumbnailFetcher(
                    (tabId, thumbnailSize, isSelected, callback) -> callback.onResult(null),
                    Tab.INVALID_TAB_ID);

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        TabGridViewBinder.setThumbnailFetcherForTesting(mNullThumbnailFetcher);
    }

    @Test
    @MediumTest
    public void testThumbnailDynamicAspectRatioWhenCaptured_FixedWhenShown() {
        // With this flag bitmap aspect ratios are not applied. Check that the resultant image views
        // still display at the right size.
        int tabCounts = 11;
        TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, tabCounts, 0, "about:blank");
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        verifyAllThumbnailHeightWithAspectRatio(tabCounts, 0.85f);

        // With hard cleanup.
        TabUiTestHelper.leaveTabSwitcher(mActivityTestRule.getActivity());
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        verifyAllThumbnailHeightWithAspectRatio(tabCounts, 0.85f);
    }

    @Test
    @MediumTest
    public void testThumbnailAspectRatio_point85() {
        int tabCounts = 11;
        TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, tabCounts, 0, "about:blank");
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        verifyAllThumbnailHeightWithAspectRatio(tabCounts, 0.85f);
    }

    @Test
    @MediumTest
    public void testThumbnail_withSoftCleanup() {
        int tabCounts = 11;
        TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, tabCounts, 0, "about:blank");
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        verifyAllThumbnailHeightWithAspectRatio(tabCounts, .85f);

        // With soft cleanup.
        TabUiTestHelper.leaveTabSwitcher(mActivityTestRule.getActivity());
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        // There is a chance this will fail without the current changes. Soft cleanup sets the
        // fetcher to null, which triggers TabGridViewBinder#releaseThumbnail. If the view still
        // under measuring, then its height can be zero after measurement.
        verifyAllThumbnailHeightWithAspectRatio(tabCounts, .85f);
    }

    private void verifyAllThumbnailHeightWithAspectRatio(int tabCounts, float ratio) {
        // The last tab is currently selected, we are at the bottom of tab switcher when it shows.
        // There is a higher chance for the test to fail with backward counting, because after the
        // view being recycled, its height might have the correct measurement.
        for (int i = tabCounts - 1; i >= 0; i--) {
            onViewWaiting(
                            allOf(
                                    isDescendantOfA(
                                            withId(
                                                    TabUiTestHelper.getTabSwitcherAncestorId(
                                                            mActivityTestRule.getActivity()))),
                                    withId(R.id.tab_list_recycler_view)))
                    .perform(scrollToPosition(i))
                    .check(ThumbnailHeightAssertion.notZeroAt(i))
                    .check(ThumbnailAspectRatioAssertion.havingAspectRatioAt(ratio, i));
        }
    }

    private static class ThumbnailAspectRatioAssertion implements ViewAssertion {
        public static ThumbnailAspectRatioAssertion havingAspectRatioAt(float ratio, int position) {
            return new ThumbnailAspectRatioAssertion(ratio, position);
        }

        private int mPosition;
        private float mExpectedRatio;

        ThumbnailAspectRatioAssertion(float ratio, int position) {
            mExpectedRatio = ratio;
            mPosition = position;
        }

        @Override
        public void check(View view, NoMatchingViewException noMatchException) {
            if (noMatchException != null) throw noMatchException;

            RecyclerView recyclerView = (RecyclerView) view;
            RecyclerView.ViewHolder viewHolder =
                    recyclerView.findViewHolderForAdapterPosition(mPosition);
            if (viewHolder != null) {
                ViewLookupCachingFrameLayout tabView =
                        (ViewLookupCachingFrameLayout) viewHolder.itemView;
                ImageView thumbnail = (ImageView) tabView.fastFindViewById(R.id.tab_thumbnail);
                float thumbnailRatio = thumbnail.getWidth() * 1.f / thumbnail.getHeight();
                assertEquals(mExpectedRatio, thumbnailRatio, 0.01);
            }
        }
    }

    private static class ThumbnailHeightAssertion implements ViewAssertion {
        public static ThumbnailHeightAssertion notZeroAt(int position) {
            return new ThumbnailHeightAssertion(position);
        }

        private int mPosition;

        ThumbnailHeightAssertion(int position) {
            mPosition = position;
        }

        @Override
        public void check(View view, NoMatchingViewException noMatchException) {
            if (noMatchException != null) throw noMatchException;

            RecyclerView recyclerView = (RecyclerView) view;
            RecyclerView.ViewHolder viewHolder =
                    recyclerView.findViewHolderForAdapterPosition(mPosition);
            if (viewHolder != null) {
                ViewLookupCachingFrameLayout tabView =
                        (ViewLookupCachingFrameLayout) viewHolder.itemView;
                ImageView thumbnail = (ImageView) tabView.fastFindViewById(R.id.tab_thumbnail);
                assertNotEquals("Thumbnail's height should not be zero", 0, thumbnail.getHeight());
            }
        }
    }
}
