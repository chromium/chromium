// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabGroupHoverCardPresenter}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupHoverCardPresenterUnitTest {
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private Tab mTab4;
    @Mock private Tab mTab5;
    @Mock private Tab mTab6;

    private Activity mActivity;
    private TabGroupHoverCardView mHoverCardView;
    private TabGroupHoverCardPresenter mPresenter;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mHoverCardView =
                (TabGroupHoverCardView)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.tab_group_hover_card_holder, null);

        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);

        when(mTab1.getTitle()).thenReturn("Tab 1");
        when(mTab2.getTitle()).thenReturn("Tab 2");
        when(mTab3.getTitle()).thenReturn("Tab 3");
        when(mTab4.getTitle()).thenReturn("Tab 4");
        when(mTab5.getTitle()).thenReturn("Tab 5");
        when(mTab6.getTitle()).thenReturn("Tab 6");

        mPresenter = new TabGroupHoverCardPresenter(mTabModelSelector);
    }

    @Test
    public void testBindData_customTitle() {
        List<Tab> tabs = List.of(mTab1, mTab2);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn("Custom Group");

        assertTrue(mPresenter.bindData(mHoverCardView, TAB_GROUP_ID));

        assertBoundData("Custom Group", /* excessCount= */ 0, /* isIncognito= */ false);
        List<String> childTitles = getVisibleChildTitles();
        assertEquals(2, childTitles.size());
        assertEquals("• Tab 1", childTitles.get(0));
        assertEquals("• Tab 2", childTitles.get(1));
    }

    @Test
    public void testBindData_fallbackTitle() {
        List<Tab> tabs = List.of(mTab1, mTab2, mTab3);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(null);

        assertTrue(mPresenter.bindData(mHoverCardView, TAB_GROUP_ID));

        assertBoundData("3 tabs", /* excessCount= */ 0, /* isIncognito= */ false);
        assertEquals(3, getVisibleChildTitles().size());
    }

    @Test
    public void testBindData_childTabsLimitAndExcessCounter() {
        List<Tab> tabs = List.of(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn("Big Group");

        assertTrue(mPresenter.bindData(mHoverCardView, TAB_GROUP_ID));

        assertBoundData("Big Group", /* excessCount= */ 1, /* isIncognito= */ false);
        List<String> childTitles = getVisibleChildTitles();
        assertEquals(5, childTitles.size());
        assertEquals("• Tab 1", childTitles.get(0));
        assertEquals("• Tab 5", childTitles.get(4));
    }

    @Test
    public void testBindData_emptyGroupReturnsFalse() {
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of());

        assertFalse(mPresenter.bindData(mHoverCardView, TAB_GROUP_ID));
    }

    @Test
    public void testBindData_nullGroupId_returnsFalse() {
        assertFalse(mPresenter.bindData(mHoverCardView, /* tabGroupId= */ null));
    }

    @Test
    public void testBindData_incognito() {
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1));
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn("Incognito Group");

        assertTrue(mPresenter.bindData(mHoverCardView, TAB_GROUP_ID));

        assertBoundData("Incognito Group", /* excessCount= */ 0, /* isIncognito= */ true);
    }

    @Test
    public void testBindData_filtersClosingAndDestroyedTabs() {
        when(mTab2.isClosing()).thenReturn(true);
        when(mTab3.isDestroyed()).thenReturn(true);
        List<Tab> tabs = List.of(mTab1, mTab2, mTab3);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(null);

        assertTrue(mPresenter.bindData(mHoverCardView, TAB_GROUP_ID));

        assertBoundData("1 tab", /* excessCount= */ 0, /* isIncognito= */ false);
        List<String> childTitles = getVisibleChildTitles();
        assertEquals(1, childTitles.size());
        assertEquals("• Tab 1", childTitles.get(0));
    }

    @Test
    public void testBindData_allTabsClosingOrDestroyed_returnsFalse() {
        when(mTab1.isClosing()).thenReturn(true);
        when(mTab2.isDestroyed()).thenReturn(true);
        List<Tab> tabs = List.of(mTab1, mTab2);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);

        assertFalse(mPresenter.bindData(mHoverCardView, TAB_GROUP_ID));
    }

    private void assertBoundData(String title, int excessCount, boolean isIncognito) {
        TextView titleView = mHoverCardView.getGroupTitleViewForTesting();
        assertEquals(title, titleView.getText().toString());
        TextView excessView = mHoverCardView.getGroupExcessTabsViewForTesting();
        if (excessCount > 0) {
            assertEquals(View.VISIBLE, excessView.getVisibility());
            assertEquals(
                    mActivity
                            .getResources()
                            .getQuantityString(
                                    R.plurals.tab_group_hover_card_excess_tabs,
                                    excessCount,
                                    excessCount),
                    excessView.getText().toString());
        } else {
            assertEquals(View.GONE, excessView.getVisibility());
        }
        assertEquals(
                TabUiThemeProvider.getTabHoverCardTextColorPrimary(mActivity, isIncognito),
                titleView.getCurrentTextColor());
    }

    private List<String> getVisibleChildTitles() {
        List<String> titles = new ArrayList<>();
        for (TextView childView : mHoverCardView.getChildTabViewsForTesting()) {
            if (childView.getVisibility() == View.VISIBLE) {
                titles.add(childView.getText().toString());
            }
        }
        return titles;
    }
}
