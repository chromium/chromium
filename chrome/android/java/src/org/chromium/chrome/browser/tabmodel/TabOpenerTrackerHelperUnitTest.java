// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;

import java.util.List;

/** Unit tests for {@link TabOpenerTrackerHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.TAB_OPENER_TRACKING})
public class TabOpenerTrackerHelperUnitTest {

    @Rule public final MockitoRule mockito = MockitoJUnit.rule();

    @Mock private Profile mProfile;

    private MockTabModel mTabModel;
    private TabOpenerTrackerHelper mHelper;
    private int mNextTabId;

    @Before
    public void setUp() {
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);
        mNextTabId = 100;
        mTabModel = spy(new MockTabModel(mProfile, null));
        mHelper = TabOpenerTrackerHelper.create();
    }

    private Tab createTab(int parentId) {
        Tab tab = mTabModel.addTab(mNextTabId++);
        tab.setParentId(parentId);
        return tab;
    }

    @Test
    public void testCreate_FlagEnabled_ReturnsHelper() {
        assertNotNull(TabOpenerTrackerHelper.create());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.TAB_OPENER_TRACKING})
    public void testCreate_FlagDisabled_ReturnsNull() {
        assertNull(TabOpenerTrackerHelper.create());
    }

    @Test
    public void testDidAddTab_ParentInRelatedSet_AddsChild() {
        Tab parentTab = createTab(Tab.INVALID_TAB_ID);
        Tab childTab = createTab(parentTab.getId());
        mHelper.addRelatedTabForTesting(parentTab.getId());

        mHelper.didAddTab(
                childTab,
                TabLaunchType.FROM_LINK,
                TabCreationState.LIVE_IN_FOREGROUND,
                /* markedForSelection= */ false);

        assertTrue(mHelper.getRelatedTabIdsForTesting().contains(childTab.getId()));
    }

    @Test
    public void testDidAddTab_ParentNotInRelatedSet_DoesNotAddChild() {
        Tab parentTab = createTab(Tab.INVALID_TAB_ID);
        Tab childTab = createTab(parentTab.getId());

        mHelper.didAddTab(
                childTab,
                TabLaunchType.FROM_LINK,
                TabCreationState.LIVE_IN_FOREGROUND,
                /* markedForSelection= */ false);

        assertFalse(mHelper.getRelatedTabIdsForTesting().contains(childTab.getId()));
    }

    @Test
    public void testDidSelectTab_ParentToChild_PreservesOpeners() {
        Tab parentTab = createTab(Tab.INVALID_TAB_ID);
        Tab childTab = createTab(parentTab.getId());

        mHelper.addRelatedTabForTesting(parentTab.getId());
        mHelper.addRelatedTabForTesting(childTab.getId());

        mHelper.didSelectTab(childTab, TabSelectionType.FROM_USER, parentTab.getId());

        assertTrue(mHelper.getRelatedTabIdsForTesting().contains(childTab.getId()));
        assertTrue(mHelper.getRelatedTabIdsForTesting().contains(parentTab.getId()));
    }

    @Test
    public void testDidSelectTab_ChildToParent_PreservesOpeners() {
        Tab parentTab = createTab(Tab.INVALID_TAB_ID);
        Tab childTab = createTab(parentTab.getId());

        mHelper.addRelatedTabForTesting(parentTab.getId());
        mHelper.addRelatedTabForTesting(childTab.getId());

        mHelper.didSelectTab(parentTab, TabSelectionType.FROM_USER, childTab.getId());

        assertTrue(mHelper.getRelatedTabIdsForTesting().contains(childTab.getId()));
        assertTrue(mHelper.getRelatedTabIdsForTesting().contains(parentTab.getId()));
    }

    @Test
    public void testDidSelectTab_SiblingToSibling_PreservesOpeners() {
        Tab parentTab = createTab(Tab.INVALID_TAB_ID);
        Tab childTab1 = createTab(parentTab.getId());
        Tab childTab2 = createTab(parentTab.getId());

        mHelper.addRelatedTabForTesting(parentTab.getId());
        mHelper.addRelatedTabForTesting(childTab1.getId());
        mHelper.addRelatedTabForTesting(childTab2.getId());

        mHelper.didSelectTab(childTab2, TabSelectionType.FROM_USER, childTab1.getId());

        assertTrue(mHelper.getRelatedTabIdsForTesting().contains(childTab1.getId()));
        assertTrue(mHelper.getRelatedTabIdsForTesting().contains(childTab2.getId()));
        assertTrue(mHelper.getRelatedTabIdsForTesting().contains(parentTab.getId()));
    }

    @Test
    public void testDidSelectTab_UnrelatedTab_ResetsOpeners() {
        Tab parentTab = createTab(Tab.INVALID_TAB_ID);
        Tab childTab = createTab(parentTab.getId());
        Tab unrelatedTab = createTab(Tab.INVALID_TAB_ID);

        mHelper.addRelatedTabForTesting(parentTab.getId());
        mHelper.addRelatedTabForTesting(childTab.getId());

        mHelper.didSelectTab(unrelatedTab, TabSelectionType.FROM_USER, childTab.getId());

        assertEquals(1, mHelper.getRelatedTabIdsForTesting().size());
        assertTrue(mHelper.getRelatedTabIdsForTesting().contains(unrelatedTab.getId()));
        assertFalse(mHelper.getRelatedTabIdsForTesting().contains(childTab.getId()));
        assertFalse(mHelper.getRelatedTabIdsForTesting().contains(parentTab.getId()));
    }

    @Test
    public void testDidSelectTab_SiblingsIgnoredAfterReset() {
        Tab parentTab = createTab(Tab.INVALID_TAB_ID);
        Tab childTab1 = createTab(parentTab.getId());
        Tab childTab2 = createTab(parentTab.getId());
        Tab unrelatedTab = createTab(Tab.INVALID_TAB_ID);

        // Initial cluster {parentTab, childTab1, childTab2}
        mHelper.addRelatedTabForTesting(parentTab.getId());
        mHelper.addRelatedTabForTesting(childTab1.getId());
        mHelper.addRelatedTabForTesting(childTab2.getId());

        // Switch to unrelated tab -> cluster reset to {unrelatedTab}
        mHelper.didSelectTab(unrelatedTab, TabSelectionType.FROM_USER, childTab1.getId());
        assertEquals(1, mHelper.getRelatedTabIdsForTesting().size());
        assertTrue(mHelper.getRelatedTabIdsForTesting().contains(unrelatedTab.getId()));

        // Switch from unrelated tab to childTab1 -> cluster reset to {childTab1}
        mHelper.didSelectTab(childTab1, TabSelectionType.FROM_USER, unrelatedTab.getId());
        assertEquals(1, mHelper.getRelatedTabIdsForTesting().size());
        assertTrue(mHelper.getRelatedTabIdsForTesting().contains(childTab1.getId()));

        // Switch from childTab1 to childTab2 (sibling): Sibling ignored as openers have reset
        mHelper.didSelectTab(childTab2, TabSelectionType.FROM_USER, childTab1.getId());
        assertEquals(1, mHelper.getRelatedTabIdsForTesting().size());
        assertTrue(mHelper.getRelatedTabIdsForTesting().contains(childTab2.getId()));
        assertFalse(mHelper.getRelatedTabIdsForTesting().contains(childTab1.getId()));
        assertFalse(mHelper.getRelatedTabIdsForTesting().contains(parentTab.getId()));
    }

    @Test
    public void testTabRemoved_RemovesFromRelatedSet() {
        Tab parent = createTab(Tab.INVALID_TAB_ID);
        Tab child = createTab(parent.getId());
        mHelper.addRelatedTabForTesting(parent.getId());
        mHelper.addRelatedTabForTesting(child.getId());

        mHelper.tabRemoved(parent);

        assertFalse(mHelper.getRelatedTabIdsForTesting().contains(parent.getId()));
        assertTrue(mHelper.getRelatedTabIdsForTesting().contains(child.getId()));
    }

    @Test
    public void testFindHierarchicalNextTab_ChildPreference() {
        Tab parentTab = createTab(Tab.INVALID_TAB_ID);
        Tab currentTab = createTab(parentTab.getId());
        Tab childTab = createTab(currentTab.getId());

        mHelper.addRelatedTabForTesting(parentTab.getId());
        mHelper.addRelatedTabForTesting(currentTab.getId());
        mHelper.addRelatedTabForTesting(childTab.getId());

        Tab nextTab = mHelper.findHierarchicalNextTab(mTabModel, currentTab, List.of(currentTab));

        assertEquals(childTab, nextTab);
    }

    @Test
    public void testFindHierarchicalNextTab_SiblingPreference() {
        Tab parentTab = createTab(Tab.INVALID_TAB_ID);
        Tab currentTab = createTab(parentTab.getId());
        Tab siblingTab = createTab(parentTab.getId());

        mHelper.addRelatedTabForTesting(parentTab.getId());
        mHelper.addRelatedTabForTesting(currentTab.getId());
        mHelper.addRelatedTabForTesting(siblingTab.getId());

        Tab nextTab = mHelper.findHierarchicalNextTab(mTabModel, currentTab, List.of(currentTab));

        assertEquals(siblingTab, nextTab);
    }

    @Test
    public void testFindHierarchicalNextTab_ParentFallback() {
        Tab parentTab = createTab(Tab.INVALID_TAB_ID);
        Tab currentTab = createTab(parentTab.getId());

        mHelper.addRelatedTabForTesting(parentTab.getId());
        mHelper.addRelatedTabForTesting(currentTab.getId());

        Tab nextTab = mHelper.findHierarchicalNextTab(mTabModel, currentTab, List.of(currentTab));

        assertEquals(parentTab, nextTab);
    }

    @Test
    public void testFindHierarchicalNextTab_SkipsCollapsedGroup() {
        Token groupId = Token.createRandom();
        Tab parentTab = createTab(Tab.INVALID_TAB_ID);
        Tab currentTab = createTab(parentTab.getId());
        Tab childTab = createTab(currentTab.getId());
        childTab.setTabGroupId(groupId);
        doReturn(true).when(mTabModel).getTabGroupCollapsed(groupId);
        assertTrue(mTabModel.getTabGroupCollapsed(childTab.getTabGroupId()));

        mHelper.addRelatedTabForTesting(parentTab.getId());
        mHelper.addRelatedTabForTesting(currentTab.getId());
        mHelper.addRelatedTabForTesting(childTab.getId());

        // Child is collapsed, so it should fall back to parent.
        Tab nextTab = mHelper.findHierarchicalNextTab(mTabModel, currentTab, List.of(currentTab));

        assertEquals(parentTab, nextTab);
    }

    @Test
    public void testFindHierarchicalNextTab_ParentInDifferentModel_Ignored() {
        int otherModelParentId = 999;
        Tab currentTab = createTab(otherModelParentId);

        mHelper.addRelatedTabForTesting(currentTab.getId());
        mHelper.addRelatedTabForTesting(otherModelParentId);

        // Parent ID is not in mTabModel, so parent lookup returns null.
        Tab nextTab = mHelper.findHierarchicalNextTab(mTabModel, currentTab, List.of(currentTab));

        assertNull(nextTab);
    }

    @Test
    public void testFindHierarchicalNextTab_ClosingTabNotInRelatedSet_ReturnsNull() {
        Tab parentTab = createTab(Tab.INVALID_TAB_ID);
        Tab currentTab = createTab(parentTab.getId());

        // currentTab is NOT in mRelatedTabIds
        Tab nextTab = mHelper.findHierarchicalNextTab(mTabModel, currentTab, List.of(currentTab));

        assertNull(nextTab);
    }

    @Test
    public void testFindHierarchicalNextTab_NoOpenerRelations_ReturnsNull() {
        Tab tab0 = createTab(Tab.INVALID_TAB_ID);

        mHelper.addRelatedTabForTesting(tab0.getId());

        Tab nextTab = mHelper.findHierarchicalNextTab(mTabModel, tab0, List.of(tab0));

        assertNull(nextTab);
    }
}
