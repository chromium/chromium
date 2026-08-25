// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;

import java.util.List;

/** Unit tests for {@link TabGroupHoverCardView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupHoverCardViewUnitTest {
    private Activity mActivity;
    private TabGroupHoverCardView mHoverCardView;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        FrameLayout root = new FrameLayout(mActivity);
        mActivity.setContentView(root);
        mHoverCardView =
                (TabGroupHoverCardView)
                        LayoutInflater.from(mActivity)
                                .inflate(
                                        R.layout.tab_group_hover_card_holder,
                                        root,
                                        /* attachToRoot= */ false);
        root.addView(mHoverCardView);
    }

    @Test
    @SmallTest
    public void testShow_setsAllFieldsAndPositions() {
        List<String> childTitles = List.of("• Tab 1", "• Tab 2");

        mHoverCardView.bindData(
                "Custom Group", childTitles, /* excessCount= */ 0, /* isIncognito= */ false);
        mHoverCardView.show(/* x= */ 50f, /* y= */ 100f);

        assertTrue(mHoverCardView.isShown());
        assertEquals(View.VISIBLE, mHoverCardView.getVisibility());
        assertEquals(
                "Custom Group", mHoverCardView.getGroupTitleViewForTesting().getText().toString());
        assertEquals(50f, mHoverCardView.getX(), 0.01f);
        assertEquals(100f, mHoverCardView.getY(), 0.01f);

        TextView[] childViews = mHoverCardView.getChildTabViewsForTesting();
        assertEquals(View.VISIBLE, childViews[0].getVisibility());
        assertEquals("• Tab 1", childViews[0].getText().toString());
        assertEquals(View.VISIBLE, childViews[1].getVisibility());
        assertEquals("• Tab 2", childViews[1].getText().toString());
        assertEquals(View.GONE, childViews[2].getVisibility());
        assertEquals(View.GONE, childViews[3].getVisibility());
        assertEquals(View.GONE, childViews[4].getVisibility());

        assertEquals(View.GONE, mHoverCardView.getGroupExcessTabsViewForTesting().getVisibility());
    }

    @Test
    @SmallTest
    public void testShow_excessCountVisibleWhenPositive() {
        List<String> childTitles = List.of("• Tab 1", "• Tab 2", "• Tab 3", "• Tab 4", "• Tab 5");

        mHoverCardView.bindData(
                "Big Group", childTitles, /* excessCount= */ 3, /* isIncognito= */ false);
        mHoverCardView.show(/* x= */ 0f, /* y= */ 0f);

        assertEquals(
                View.VISIBLE, mHoverCardView.getGroupExcessTabsViewForTesting().getVisibility());
        assertEquals(
                "+3 more", mHoverCardView.getGroupExcessTabsViewForTesting().getText().toString());
    }

    @Test
    @SmallTest
    public void testShow_incognitoColorsApplied() {
        mHoverCardView.bindData(
                "Incognito Group",
                List.of("• Tab 1"),
                /* excessCount= */ 0,
                /* isIncognito= */ true);
        mHoverCardView.show(/* x= */ 0f, /* y= */ 0f);

        assertEquals(
                TabUiThemeProvider.getTabHoverCardTextColorPrimary(
                        mActivity, /* isIncognito= */ true),
                mHoverCardView.getGroupTitleViewForTesting().getCurrentTextColor());
        assertEquals(
                TabUiThemeProvider.getTabHoverCardTextColorSecondary(
                        mActivity, /* isIncognito= */ true),
                mHoverCardView.getChildTabViewsForTesting()[0].getCurrentTextColor());
        assertEquals(
                TabUiThemeProvider.getTabHoverCardBackgroundTintList(
                        mActivity, /* isIncognito= */ true),
                mHoverCardView.getBackgroundTintListForTesting());
    }

    @Test
    @SmallTest
    public void testShow_childTabsCappedAtMaxPreviewTabs() {
        List<String> manyTabs = List.of("• T1", "• T2", "• T3", "• T4", "• T5", "• T6", "• T7");
        mHoverCardView.bindData(
                "Large Group", manyTabs, /* excessCount= */ 2, /* isIncognito= */ false);
        mHoverCardView.show(/* x= */ 0f, /* y= */ 0f);

        TextView[] childViews = mHoverCardView.getChildTabViewsForTesting();
        for (int i = 0; i < TabGroupHoverCardView.MAX_PREVIEW_TABS; i++) {
            assertEquals(View.VISIBLE, childViews[i].getVisibility());
            assertEquals(manyTabs.get(i), childViews[i].getText().toString());
        }
    }

    @Test
    @SmallTest
    public void testHide() {
        List<String> childTitles = List.of("• Tab 1");

        mHoverCardView.bindData(
                "Group", childTitles, /* excessCount= */ 0, /* isIncognito= */ false);
        mHoverCardView.show(/* x= */ 0f, /* y= */ 0f);
        assertTrue(mHoverCardView.isShown());

        mHoverCardView.hide();
        assertFalse(mHoverCardView.isShown());
        assertEquals(View.GONE, mHoverCardView.getVisibility());
    }

    @Test
    @SmallTest
    public void testDestroy() {
        mHoverCardView.destroy();
        assertFalse(mHoverCardView.isShown());
    }

    @Test
    @SmallTest
    public void testOnMeasure_hugsContentAndCapsAtMaxWidth() {
        int expectedMaxWidth = TabHoverCardView.getHoverCardWidthPx(mActivity);

        // Short content should hug the content (measured width <= expectedMaxWidth).
        mHoverCardView.bindData(
                "Short", List.of("• A"), /* excessCount= */ 0, /* isIncognito= */ false);
        mHoverCardView.show(/* x= */ 0f, /* y= */ 0f);

        mHoverCardView.measure(
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
        assertTrue(mHoverCardView.getMeasuredWidth() <= expectedMaxWidth);

        // Extremely long content should be capped at expectedMaxWidth.
        String longTitle = "A".repeat(500);
        mHoverCardView.bindData(
                longTitle,
                List.of("• " + longTitle),
                /* excessCount= */ 0,
                /* isIncognito= */ false);
        mHoverCardView.show(/* x= */ 0f, /* y= */ 0f);

        mHoverCardView.measure(
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
        assertEquals(expectedMaxWidth, mHoverCardView.getMeasuredWidth());
    }
}
