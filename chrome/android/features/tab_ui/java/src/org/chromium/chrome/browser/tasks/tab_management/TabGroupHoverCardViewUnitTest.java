// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.tab_groups.TabGroupId;

import java.util.List;

/** Unit tests for {@link TabGroupHoverCardView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupHoverCardViewUnitTest {
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;

    private final SettableMonotonicObservableSupplier<TabModel> mTabModelSupplier =
            ObservableSuppliers.createMonotonic();

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

        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mTabModelSupplier);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        when(mTab1.getTitle()).thenReturn("Tab 1");
        when(mTab2.getTitle()).thenReturn("Tab 2");
    }

    @Test
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
    public void testShow_incognitoColorsApplied() {
        mHoverCardView.bindData(
                "Incognito Group",
                List.of("• Tab 1"),
                /* excessCount= */ 0,
                /* isIncognito= */ true);
        mHoverCardView.show(/* x= */ 0f, /* y= */ 0f);

        assertHoverCardColors(/* isIncognito= */ true);
    }

    @Test
    public void testInitialize() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);
        mHoverCardView.initialize(mTabModelSelector);
        assertHoverCardColors(/* isIncognito= */ true);

        TabModel standardTabModel = mock(TabModel.class);
        TabModel incognitoTabModel = mock(TabModel.class);
        when(standardTabModel.isIncognitoBranded()).thenReturn(false);
        when(incognitoTabModel.isIncognitoBranded()).thenReturn(true);

        mTabModelSupplier.set(standardTabModel);
        assertHoverCardColors(/* isIncognito= */ false);

        mTabModelSupplier.set(incognitoTabModel);
        assertHoverCardColors(/* isIncognito= */ true);
    }

    @Test
    public void testBindData_withToken() {
        when(mTabModel.getTabsInGroup(TabGroupId.fromRawToken(TAB_GROUP_ID)))
                .thenReturn(List.of(mTab1, mTab2));
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn("Token Group");

        assertFalse(mHoverCardView.bindData(TAB_GROUP_ID));

        mHoverCardView.initialize(mTabModelSelector);
        assertFalse(mHoverCardView.bindData(/* tabGroupId= */ null));
        assertTrue(mHoverCardView.bindData(TAB_GROUP_ID));

        assertEquals(
                "Token Group", mHoverCardView.getGroupTitleViewForTesting().getText().toString());
        TextView[] childViews = mHoverCardView.getChildTabViewsForTesting();
        assertEquals(View.VISIBLE, childViews[0].getVisibility());
        assertEquals("• Tab 1", childViews[0].getText().toString());
        assertEquals(View.VISIBLE, childViews[1].getVisibility());
        assertEquals("• Tab 2", childViews[1].getText().toString());
        assertEquals(View.GONE, childViews[2].getVisibility());
        assertEquals(View.GONE, mHoverCardView.getGroupExcessTabsViewForTesting().getVisibility());
    }

    @Test
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
    public void testDestroy() {
        when(mTabModel.getTabsInGroup(TabGroupId.fromRawToken(TAB_GROUP_ID)))
                .thenReturn(List.of(mTab1));
        mHoverCardView.initialize(mTabModelSelector);
        assertTrue(mHoverCardView.bindData(TAB_GROUP_ID));
        mHoverCardView.show(/* x= */ 10f, /* y= */ 20f);
        assertTrue(mHoverCardView.isShowing());
        assertTrue(mHoverCardView.isShown());
        assertTrue(mTabModelSupplier.hasObservers());

        mHoverCardView.destroy();
        assertFalse(mHoverCardView.isShowing());
        assertFalse(mHoverCardView.isShown());
        assertFalse(mTabModelSupplier.hasObservers());
        assertFalse(mHoverCardView.bindData(TAB_GROUP_ID));
    }

    @Test
    public void testOnMeasure_respectsMinAndMaxWidth() {
        int expectedMaxWidth = HoverCardView.getHoverCardWidthPx(mActivity);
        int minContentWidth =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_group_hover_card_min_width);

        // Short content should hug the content while respecting minContentWidth.
        mHoverCardView.bindData(
                "Short", List.of("• A"), /* excessCount= */ 0, /* isIncognito= */ false);
        mHoverCardView.show(/* x= */ 0f, /* y= */ 0f);

        mHoverCardView.measure(
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
        assertTrue(mHoverCardView.getMeasuredWidth() <= expectedMaxWidth);
        assertTrue(
                mHoverCardView.findViewById(R.id.content_view).getMeasuredWidth()
                        >= minContentWidth);

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

    private void assertHoverCardColors(boolean isIncognito) {
        assertEquals(
                TabUiThemeProvider.getTabHoverCardTextColorPrimary(mActivity, isIncognito),
                mHoverCardView.getGroupTitleViewForTesting().getCurrentTextColor());
        assertEquals(
                TabUiThemeProvider.getTabHoverCardTextColorSecondary(mActivity, isIncognito),
                mHoverCardView.getChildTabViewsForTesting()[0].getCurrentTextColor());
        assertEquals(
                TabUiThemeProvider.getTabHoverCardBackgroundTintList(mActivity, isIncognito),
                mHoverCardView.getBackgroundTintList());
    }
}
