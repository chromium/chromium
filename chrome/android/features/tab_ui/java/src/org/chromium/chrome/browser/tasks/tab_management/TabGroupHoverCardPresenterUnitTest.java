// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
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

import java.util.List;

/** Unit tests for {@link TabGroupHoverCardPresenter}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupHoverCardPresenterUnitTest {
    private static final int HEADER_TAB_ID = 100;
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private TabGroupHoverCardView mHoverCardView;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private Tab mTab4;
    @Mock private Tab mTab5;
    @Mock private Tab mTab6;

    @Captor private ArgumentCaptor<List<String>> mChildTitlesCaptor;

    private TabGroupHoverCardPresenter mPresenter;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);

        when(mHoverCardView.getContext()).thenReturn(activity);

        when(mTab1.getTitle()).thenReturn("Tab 1");
        when(mTab2.getTitle()).thenReturn("Tab 2");
        when(mTab3.getTitle()).thenReturn("Tab 3");
        when(mTab4.getTitle()).thenReturn("Tab 4");
        when(mTab5.getTitle()).thenReturn("Tab 5");
        when(mTab6.getTitle()).thenReturn("Tab 6");

        mPresenter = new TabGroupHoverCardPresenter(mTabModelSelector);
    }

    @Test
    @SmallTest
    public void testBindData_customTitle() {
        List<Tab> tabs = List.of(mTab1, mTab2);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn("Custom Group");

        assertTrue(mPresenter.bindData(mHoverCardView, HEADER_TAB_ID, TAB_GROUP_ID));

        verify(mHoverCardView)
                .bindData(
                        eq("Custom Group"),
                        mChildTitlesCaptor.capture(),
                        /* excessCount= */ eq(0),
                        /* isIncognito= */ eq(false));
        List<String> capturedTitles = mChildTitlesCaptor.getValue();
        assertEquals(2, capturedTitles.size());
        assertEquals("• Tab 1", capturedTitles.get(0));
        assertEquals("• Tab 2", capturedTitles.get(1));
    }

    @Test
    @SmallTest
    public void testBindData_fallbackTitle() {
        List<Tab> tabs = List.of(mTab1, mTab2, mTab3);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(null);

        assertTrue(mPresenter.bindData(mHoverCardView, HEADER_TAB_ID, TAB_GROUP_ID));

        verify(mHoverCardView)
                .bindData(
                        eq("3 tabs"),
                        mChildTitlesCaptor.capture(),
                        /* excessCount= */ eq(0),
                        /* isIncognito= */ eq(false));
        assertEquals(3, mChildTitlesCaptor.getValue().size());
    }

    @Test
    @SmallTest
    public void testBindData_childTabsLimitAndExcessCounter() {
        List<Tab> tabs = List.of(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn("Big Group");

        assertTrue(mPresenter.bindData(mHoverCardView, HEADER_TAB_ID, TAB_GROUP_ID));

        verify(mHoverCardView)
                .bindData(
                        eq("Big Group"),
                        mChildTitlesCaptor.capture(),
                        /* excessCount= */ eq(1),
                        /* isIncognito= */ eq(false));
        List<String> capturedTitles = mChildTitlesCaptor.getValue();
        assertEquals(5, capturedTitles.size());
        assertEquals("• Tab 1", capturedTitles.get(0));
        assertEquals("• Tab 5", capturedTitles.get(4));
    }

    @Test
    @SmallTest
    public void testBindData_emptyGroupReturnsFalse() {
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of());

        assertFalse(mPresenter.bindData(mHoverCardView, HEADER_TAB_ID, TAB_GROUP_ID));
    }

    @Test
    @SmallTest
    public void testBindData_nullGroupId_resolvesFromHeaderTabId() {
        when(mTabModel.getTabById(HEADER_TAB_ID)).thenReturn(mTab1);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1));
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn("Resolved Group");

        assertTrue(mPresenter.bindData(mHoverCardView, HEADER_TAB_ID, /* tabGroupId= */ null));

        verify(mHoverCardView)
                .bindData(
                        eq("Resolved Group"),
                        /* childTabTitles= */ anyList(),
                        /* excessCount= */ eq(0),
                        /* isIncognito= */ eq(false));
    }

    @Test
    @SmallTest
    public void testBindData_nullGroupIdAndInvalidHeaderId_returnsFalse() {
        assertFalse(
                mPresenter.bindData(mHoverCardView, Tab.INVALID_TAB_ID, /* tabGroupId= */ null));
    }

    @Test
    @SmallTest
    public void testBindData_incognito() {
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1));
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn("Incognito Group");

        assertTrue(mPresenter.bindData(mHoverCardView, HEADER_TAB_ID, TAB_GROUP_ID));

        verify(mHoverCardView)
                .bindData(
                        eq("Incognito Group"),
                        /* childTabTitles= */ anyList(),
                        /* excessCount= */ eq(0),
                        /* isIncognito= */ eq(true));
    }

    @Test
    @SmallTest
    public void testBindData_filtersClosingAndDestroyedTabs() {
        when(mTab2.isClosing()).thenReturn(true);
        when(mTab3.isDestroyed()).thenReturn(true);
        List<Tab> tabs = List.of(mTab1, mTab2, mTab3);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(null);

        assertTrue(mPresenter.bindData(mHoverCardView, HEADER_TAB_ID, TAB_GROUP_ID));

        verify(mHoverCardView)
                .bindData(
                        eq("1 tab"),
                        mChildTitlesCaptor.capture(),
                        /* excessCount= */ eq(0),
                        /* isIncognito= */ eq(false));
        List<String> capturedTitles = mChildTitlesCaptor.getValue();
        assertEquals(1, capturedTitles.size());
        assertEquals("• Tab 1", capturedTitles.get(0));
    }

    @Test
    @SmallTest
    public void testBindData_allTabsClosingOrDestroyed_returnsFalse() {
        when(mTab1.isClosing()).thenReturn(true);
        when(mTab2.isDestroyed()).thenReturn(true);
        List<Tab> tabs = List.of(mTab1, mTab2);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);

        assertFalse(mPresenter.bindData(mHoverCardView, HEADER_TAB_ID, TAB_GROUP_ID));
    }
}
