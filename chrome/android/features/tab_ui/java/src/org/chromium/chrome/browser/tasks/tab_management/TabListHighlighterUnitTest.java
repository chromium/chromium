// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.ALL_KEYS_TAB_GRID;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabCardHighlightState;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collections;
import java.util.Set;

/** Unit tests for {@link TabListHighlighter}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabListHighlighterUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private MVCListAdapter.ModelList mModelList;
    private TabListHighlighter mHighlightDelegate;

    private PropertyModel mModel1;
    private PropertyModel mModel2;
    private PropertyModel mModel3;
    private PropertyModel mNonTabModel;

    @Before
    public void setUp() {
        mModelList = new MVCListAdapter.ModelList();
        mHighlightDelegate = new TabListHighlighter(mModelList);

        mModel1 =
                new PropertyModel.Builder(ALL_KEYS_TAB_GRID).with(TabProperties.TAB_ID, 10).build();
        mModel2 =
                new PropertyModel.Builder(ALL_KEYS_TAB_GRID).with(TabProperties.TAB_ID, 20).build();
        mModel3 =
                new PropertyModel.Builder(ALL_KEYS_TAB_GRID).with(TabProperties.TAB_ID, 30).build();
        mNonTabModel = new PropertyModel.Builder(ALL_KEYS_TAB_GRID).build();

        mModelList.add(new ListItem(0, mModel1));
        mModelList.add(new ListItem(0, mModel2));
        mModelList.add(new ListItem(0, mNonTabModel));
        mModelList.add(new ListItem(0, mModel3));
    }

    @Test
    public void testHighlightTabs() {
        mModel1.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.NOT_HIGHLIGHTED);
        mModel2.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.NOT_HIGHLIGHTED);
        mModel3.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.NOT_HIGHLIGHTED);
        mNonTabModel.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.NOT_HIGHLIGHTED);

        Set<Integer> tabIdsToHighlight = Set.of(10, 30);
        mHighlightDelegate.highlightTabs(tabIdsToHighlight);

        assertEquals(
                TabCardHighlightState.TO_BE_HIGHLIGHTED,
                mModel1.get(TabProperties.HIGHLIGHT_STATE));
        assertEquals(
                TabCardHighlightState.NOT_HIGHLIGHTED, mModel2.get(TabProperties.HIGHLIGHT_STATE));
        assertEquals(
                TabCardHighlightState.TO_BE_HIGHLIGHTED,
                mModel3.get(TabProperties.HIGHLIGHT_STATE));
        assertEquals(
                TabCardHighlightState.NOT_HIGHLIGHTED,
                mNonTabModel.get(TabProperties.HIGHLIGHT_STATE));
    }

    @Test
    public void testHighlightTabs_emptyList() {
        mModel1.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.NOT_HIGHLIGHTED);
        mModel2.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.NOT_HIGHLIGHTED);
        mModel3.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.NOT_HIGHLIGHTED);

        Set<Integer> tabIdsToHighlight = Collections.emptySet();
        mHighlightDelegate.highlightTabs(tabIdsToHighlight);

        assertEquals(
                TabCardHighlightState.NOT_HIGHLIGHTED, mModel1.get(TabProperties.HIGHLIGHT_STATE));
        assertEquals(
                TabCardHighlightState.NOT_HIGHLIGHTED, mModel2.get(TabProperties.HIGHLIGHT_STATE));
        assertEquals(
                TabCardHighlightState.NOT_HIGHLIGHTED, mModel3.get(TabProperties.HIGHLIGHT_STATE));
    }

    @Test
    public void testHighlightTabs_noMatchingIds() {
        mModel1.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.NOT_HIGHLIGHTED);
        mModel2.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.NOT_HIGHLIGHTED);
        mModel3.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.NOT_HIGHLIGHTED);

        Set<Integer> tabIdsToHighlight = Set.of(99, 101);
        mHighlightDelegate.highlightTabs(tabIdsToHighlight);

        assertEquals(
                TabCardHighlightState.NOT_HIGHLIGHTED, mModel1.get(TabProperties.HIGHLIGHT_STATE));
        assertEquals(
                TabCardHighlightState.NOT_HIGHLIGHTED, mModel2.get(TabProperties.HIGHLIGHT_STATE));
        assertEquals(
                TabCardHighlightState.NOT_HIGHLIGHTED, mModel3.get(TabProperties.HIGHLIGHT_STATE));
    }

    @Test
    public void testUnhighlightTabs() {
        mModel1.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.TO_BE_HIGHLIGHTED);
        mModel2.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.NOT_HIGHLIGHTED);
        mModel3.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.HIGHLIGHTED);

        mHighlightDelegate.unhighlightTabs();

        assertEquals(
                TabCardHighlightState.NOT_HIGHLIGHTED, mModel1.get(TabProperties.HIGHLIGHT_STATE));
        assertEquals(
                TabCardHighlightState.NOT_HIGHLIGHTED, mModel2.get(TabProperties.HIGHLIGHT_STATE));
        assertEquals(
                TabCardHighlightState.NOT_HIGHLIGHTED, mModel3.get(TabProperties.HIGHLIGHT_STATE));
    }

    @Test
    public void testUnhighlightTabs_emptyModelList() {
        mModelList.clear();
        mHighlightDelegate.unhighlightTabs();
    }
}
