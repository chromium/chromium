// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.TabAttributeKeys;
import org.chromium.chrome.browser.tab.TabAttributes;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;

/** Unit tests for {@link TabModelOrderControllerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabModelOrderControllerImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;

    private MockTabModelSelector mTabModelSelector;
    private TabModel mTabModel;
    private TabModel mOtherModel;
    private TabModelOrderControllerImpl mOrderController;

    @Before
    public void setUp() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mProfile.isIncognitoBranded()).thenReturn(false);
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        when(mIncognitoProfile.isIncognitoBranded()).thenReturn(true);

        mTabModelSelector = new MockTabModelSelector(mProfile, mIncognitoProfile, 0, 0, null);
        mTabModel = mTabModelSelector.getModel(false);
        mOtherModel = mTabModelSelector.getModel(true);
        mOrderController = new TabModelOrderControllerImpl(mTabModelSelector);
    }

    private MockTab addTab(TabModel model, int id, boolean isPinned) {
        MockTab tab = new MockTab(id, model.getProfile());
        tab.setIsPinned(isPinned);
        model.addTab(
                tab,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        return tab;
    }

    @Test
    public void testDetermineInsertionIndex_InvalidLaunchTypes() {
        MockTab newTab = new MockTab(1, mProfile);
        assertEquals(
                TabList.INVALID_TAB_INDEX,
                mOrderController.determineInsertionIndex(
                        TabLaunchType.FROM_BROWSER_ACTIONS, 0, newTab));
        assertEquals(
                TabList.INVALID_TAB_INDEX,
                mOrderController.determineInsertionIndex(
                        TabLaunchType.FROM_RECENT_TABS, 0, newTab));
    }

    @Test
    public void testDetermineInsertionIndex_PinnedTab_ValidPosition() {
        addTab(mTabModel, 1, /* isPinned= */ true);
        addTab(mTabModel, 2, /* isPinned= */ true);
        addTab(mTabModel, 3, /* isPinned= */ false);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER);

        MockTab newTab = new MockTab(4, mProfile);
        newTab.setIsPinned(true);

        // Position 1 is <= findFirstNonPinnedTabIndex (which is 2).
        int position = mOrderController.determineInsertionIndex(TabLaunchType.FROM_LINK, 1, newTab);
        assertEquals(1, position);
    }

    @Test
    public void testDetermineInsertionIndex_PinnedTab_PositionGreaterThanFirstNonPinned() {
        addTab(mTabModel, 1, /* isPinned= */ true);
        addTab(mTabModel, 2, /* isPinned= */ true);
        addTab(mTabModel, 3, /* isPinned= */ false);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER);

        MockTab newTab = new MockTab(4, mProfile);
        newTab.setIsPinned(true);

        // Position 3 is > findFirstNonPinnedTabIndex (which is 2).
        int position = mOrderController.determineInsertionIndex(TabLaunchType.FROM_LINK, 3, newTab);
        assertEquals(TabList.INVALID_TAB_INDEX, position);
    }

    @Test
    public void testDetermineInsertionIndex_PinnedTab_FromTabListInterface_WithPinnedParent() {
        addTab(mTabModel, 1, /* isPinned= */ true);
        MockTab parentTab = addTab(mTabModel, 2, /* isPinned= */ true);
        addTab(mTabModel, 3, /* isPinned= */ false);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER);

        MockTab newTab = new MockTab(4, mProfile);
        newTab.setIsPinned(true);
        newTab.setParentId(parentTab.getId());

        int position =
                mOrderController.determineInsertionIndex(
                        TabLaunchType.FROM_TAB_LIST_INTERFACE, 0, newTab);
        // Parent tab is at index 1 and is pinned, so should insert at parentIndex + 1 = 2.
        assertEquals(2, position);
    }

    @Test
    public void testDetermineInsertionIndex_PinnedTab_FromTabListInterface_WithNonPinnedParent() {
        addTab(mTabModel, 1, /* isPinned= */ true);
        MockTab parentTab = addTab(mTabModel, 2, /* isPinned= */ false);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER);

        MockTab newTab = new MockTab(3, mProfile);
        newTab.setIsPinned(true);
        newTab.setParentId(parentTab.getId());

        int position =
                mOrderController.determineInsertionIndex(
                        TabLaunchType.FROM_TAB_LIST_INTERFACE, 1, newTab);
        // Parent tab is not pinned, so it falls back to position check: 1 <= firstNonPinned (1).
        assertEquals(1, position);
    }

    @Test
    public void testDetermineInsertionIndex_PinnedTab_FromRestore() {
        addTab(mTabModel, 1, /* isPinned= */ true);
        addTab(mTabModel, 2, /* isPinned= */ false);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER);

        MockTab newTab = new MockTab(3, mProfile);
        newTab.setIsPinned(true);

        // FROM_RESTORE ignores pinned tab branch and returns position directly.
        int position =
                mOrderController.determineInsertionIndex(TabLaunchType.FROM_RESTORE, 5, newTab);
        assertEquals(5, position);
    }

    @Test
    public void testDetermineInsertionIndex_DifferentModelType() {
        addTab(mTabModel, 1, /* isPinned= */ false);
        addTab(mTabModel, 2, /* isPinned= */ false);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER);

        addTab(mOtherModel, 101, /* isPinned= */ false);
        addTab(mOtherModel, 102, /* isPinned= */ false);
        addTab(mOtherModel, 103, /* isPinned= */ false);

        MockTab newIncognitoTab = new MockTab(104, mIncognitoProfile);

        int position =
                mOrderController.determineInsertionIndex(
                        TabLaunchType.FROM_LINK, TabList.INVALID_TAB_INDEX, newIncognitoTab);
        assertEquals(
                "Opening tab in different model type should insert at end of target model",
                3,
                position);
    }

    @Test
    public void testDetermineInsertionIndex_EmptyModel() {
        MockTab newTab = new MockTab(1, mProfile);
        int position =
                mOrderController.determineInsertionIndex(
                        TabLaunchType.FROM_LINK, TabList.INVALID_TAB_INDEX, newTab);
        assertEquals("Opening tab in empty model should return 0", 0, position);
    }

    @Test
    public void testDetermineInsertionIndex_CurrentTabIsPinned() {
        addTab(mTabModel, 1, /* isPinned= */ true);
        addTab(mTabModel, 2, /* isPinned= */ true);
        addTab(mTabModel, 3, /* isPinned= */ false);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER);

        MockTab newTab = new MockTab(4, mProfile);
        int position =
                mOrderController.determineInsertionIndex(
                        TabLaunchType.FROM_LINK, TabList.INVALID_TAB_INDEX, newTab);
        assertEquals("Should insert after last pinned tab", 2, position);
    }

    @Test
    public void testDetermineInsertionIndex_Foreground_AdjacentToParentTab() {
        addTab(mTabModel, 1, /* isPinned= */ false);
        MockTab parentTab = addTab(mTabModel, 2, /* isPinned= */ false);
        addTab(mTabModel, 3, /* isPinned= */ false);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER);

        MockTab newTab = new MockTab(4, mProfile);
        newTab.setParentId(parentTab.getId());

        int position =
                mOrderController.determineInsertionIndex(
                        TabLaunchType.FROM_LINK, TabList.INVALID_TAB_INDEX, newTab);
        assertEquals("Should insert adjacent to parent tab", 2, position);
    }

    @Test
    public void testDetermineInsertionIndex_Foreground_AdjacentToCurrentTab() {
        MockTab currentTab = addTab(mTabModel, 1, /* isPinned= */ false);
        addTab(mTabModel, 2, /* isPinned= */ false);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER);

        MockTab newTab = new MockTab(3, mProfile);
        newTab.setParentId(currentTab.getId());

        int position =
                mOrderController.determineInsertionIndex(
                        TabLaunchType.FROM_LINK, TabList.INVALID_TAB_INDEX, newTab);
        assertEquals(
                "Should insert adjacent to current tab when parent is current tab", 1, position);
    }

    @Test
    public void testDetermineInsertionIndex_Foreground_AdjacentToCurrentTab_NoParent() {
        addTab(mTabModel, 1, /* isPinned= */ false);
        addTab(mTabModel, 2, /* isPinned= */ false);
        mTabModel.setIndex(1, TabSelectionType.FROM_USER);

        MockTab newTab = new MockTab(3, mProfile);

        int position =
                mOrderController.determineInsertionIndex(
                        TabLaunchType.FROM_LINK, TabList.INVALID_TAB_INDEX, newTab);
        assertEquals("Should insert adjacent to current tab when no parent", 2, position);
    }

    @Test
    public void testDetermineInsertionIndex_Background_AdjacentToPreviousBackgroundTabs() {
        MockTab currentTab = addTab(mTabModel, 1, /* isPinned= */ false);
        MockTab bgTab1 = addTab(mTabModel, 2, /* isPinned= */ false);
        bgTab1.setParentId(currentTab.getId());
        TabAttributes.from(bgTab1).set(TabAttributeKeys.GROUPED_WITH_PARENT, true);

        MockTab bgTab2 = addTab(mTabModel, 3, /* isPinned= */ false);
        bgTab2.setParentId(currentTab.getId());
        TabAttributes.from(bgTab2).set(TabAttributeKeys.GROUPED_WITH_PARENT, true);

        addTab(mTabModel, 4, /* isPinned= */ false);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER);

        MockTab newTab = new MockTab(5, mProfile);
        newTab.setParentId(currentTab.getId());

        int position =
                mOrderController.determineInsertionIndex(
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, TabList.INVALID_TAB_INDEX, newTab);
        assertEquals("Should insert after previous background tabs of same opener", 3, position);
    }

    @Test
    public void testDetermineInsertionIndex_Background_NoPreviousBackgroundTabs() {
        MockTab currentTab = addTab(mTabModel, 1, /* isPinned= */ false);
        addTab(mTabModel, 2, /* isPinned= */ false);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER);

        MockTab newTab = new MockTab(3, mProfile);
        newTab.setParentId(currentTab.getId());

        int position =
                mOrderController.determineInsertionIndex(
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, TabList.INVALID_TAB_INDEX, newTab);
        assertEquals(
                "Should insert adjacent to current tab when no prior background tabs", 1, position);
    }

    @Test
    public void testDetermineInsertionIndex_Foreground_ForgetsOpeners() {
        MockTab currentTab = addTab(mTabModel, 1, /* isPinned= */ false);
        MockTab bgTab = addTab(mTabModel, 2, /* isPinned= */ false);
        bgTab.setParentId(currentTab.getId());
        TabAttributes.from(bgTab).set(TabAttributeKeys.GROUPED_WITH_PARENT, true);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER);

        MockTab newForegroundTab = new MockTab(3, mProfile);
        mOrderController.determineInsertionIndex(
                TabLaunchType.FROM_LINK, TabList.INVALID_TAB_INDEX, newForegroundTab);

        assertFalse(
                "Opening in foreground should forget all previous openers",
                TabAttributes.from(bgTab).get(TabAttributeKeys.GROUPED_WITH_PARENT, true));
    }

    @Test
    public void testWillOpenInForeground() {
        assertTrue(
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_LINK, false, false));
        assertTrue(
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_LONGPRESS_FOREGROUND, false, false));
        assertTrue(
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP, false, false));
        assertTrue(
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_LONGPRESS_INCOGNITO, false, false));
        assertTrue(
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_HISTORY_NAVIGATION_FOREGROUND, false, false));

        assertFalse(
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_RESTORE, false, false));
        assertFalse(
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_BROWSER_ACTIONS, false, false));
        assertFalse(
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_RESTORE_TABS_UI, false, false));
        assertFalse(
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, false, false));
        assertFalse(
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP, false, false));
        assertFalse(
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_RECENT_TABS, false, false));
        assertFalse(
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_SYNC_BACKGROUND, false, false));
        assertFalse(
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP, false, false));
        assertFalse(
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND, false, false));
        assertFalse(
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_REPARENTING_BACKGROUND, false, false));
        assertFalse(
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND, false, false));
        assertFalse(
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_TAB_LIST_INTERFACE_BACKGROUND, false, false));

        assertTrue(
                "Opening background tab with model mismatch opens in foreground",
                TabModelOrderControllerImpl.willOpenInForeground(
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, true, false));

        assertTrue(mOrderController.willOpenInForeground(TabLaunchType.FROM_LINK, false));
        assertFalse(
                mOrderController.willOpenInForeground(
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, false));
    }

    @Test
    public void testSameModelType() {
        MockTab regularTab = new MockTab(1, mProfile);
        MockTab incognitoTab = new MockTab(2, mIncognitoProfile);

        assertTrue(TabModelOrderControllerImpl.sameModelType(mTabModel, regularTab));
        assertFalse(TabModelOrderControllerImpl.sameModelType(mTabModel, incognitoTab));
        assertTrue(TabModelOrderControllerImpl.sameModelType(mOtherModel, incognitoTab));
        assertFalse(TabModelOrderControllerImpl.sameModelType(mOtherModel, regularTab));
    }

    @Test
    public void testMightBeAdjacent() {
        assertTrue(TabModelOrderControllerImpl.mightBeAdjacent(TabLaunchType.FROM_LINK));
        assertTrue(
                TabModelOrderControllerImpl.mightBeAdjacent(
                        TabLaunchType.FROM_LONGPRESS_FOREGROUND));
        assertTrue(
                TabModelOrderControllerImpl.mightBeAdjacent(
                        TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP));
        assertTrue(
                TabModelOrderControllerImpl.mightBeAdjacent(
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND));
        assertTrue(
                TabModelOrderControllerImpl.mightBeAdjacent(
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP));
        assertTrue(
                TabModelOrderControllerImpl.mightBeAdjacent(
                        TabLaunchType.FROM_LONGPRESS_INCOGNITO));
        assertTrue(
                TabModelOrderControllerImpl.mightBeAdjacent(
                        TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND));
        assertTrue(
                TabModelOrderControllerImpl.mightBeAdjacent(
                        TabLaunchType.FROM_HISTORY_NAVIGATION_FOREGROUND));

        assertFalse(TabModelOrderControllerImpl.mightBeAdjacent(TabLaunchType.FROM_RESTORE));
        assertFalse(TabModelOrderControllerImpl.mightBeAdjacent(TabLaunchType.FROM_CHROME_UI));
        assertFalse(TabModelOrderControllerImpl.mightBeAdjacent(TabLaunchType.FROM_STARTUP));
    }
}
