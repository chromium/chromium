// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin.TAB_STRIP_CONTEXT_MENU;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

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
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator.OnItemClickedCallback;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.List;

/** Unit tests for {@link TabContextMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.DATA_SHARING})
public class TabContextMenuCoordinatorUnitTest {
    private static final int TAB_ID = 1;
    private static final int TAB_OUTSIDE_OF_GROUP_ID = 2;
    private static final int NON_URL_TAB_ID = 3;
    private static final Token TAB_GROUP_ID = Token.createRandom();
    private static final String COLLABORATION_ID = "CollaborationId";
    private static final GURL EXAMPLE_URL = new GURL("https://example.com");
    private static final GURL CHROME_SCHEME_URL = new GURL("chrome://history");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private TabContextMenuCoordinator mTabContextMenuCoordinator;
    private OnItemClickedCallback<Integer> mOnItemClickedCallback;
    private MockTabModel mTabModel;
    private final SavedTabGroup mSavedTabGroup = new SavedTabGroup();
    @Mock private Tab mTab1;
    @Mock private Tab mTabOutsideOfGroup;
    @Mock private Tab mNonUrlTab;
    @Mock private TabRemover mTabRemover;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private Profile mProfile;
    @Mock private TabGroupListBottomSheetCoordinator mBottomSheetCoordinator;
    @Mock private ShareDelegate mShareDelegate;
    @Mock private TabCreator mTabCreator;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ServiceStatus mServiceStatus;
    @Mock private WeakReference<Activity> mWeakReferenceActivity;

    @Before
    public void setUp() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        when(mServiceStatus.isAllowedToCreate()).thenReturn(true);
        CollaborationServiceFactory.setForTesting(mCollaborationService);

        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mWindowAndroid.getKeyboardDelegate()).thenReturn(mKeyboardVisibilityDelegate);
        when(mWindowAndroid.getActivity()).thenReturn(mWeakReferenceActivity);
        when(mWeakReferenceActivity.get()).thenReturn(activity);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabModel.isIncognito()).thenReturn(false);
        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab1);
        when(mTabModel.getTabById(TAB_OUTSIDE_OF_GROUP_ID)).thenReturn(mTabOutsideOfGroup);
        when(mTabModel.getTabById(NON_URL_TAB_ID)).thenReturn(mNonUrlTab);
        mTabModel.setTabRemoverForTesting(mTabRemover);
        mTabModel.setTabCreatorForTesting(mTabCreator);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTab1.getUrl()).thenReturn(EXAMPLE_URL);
        when(mTabOutsideOfGroup.getTabGroupId()).thenReturn(null);
        when(mTabOutsideOfGroup.getUrl()).thenReturn(EXAMPLE_URL);
        when(mNonUrlTab.getTabGroupId()).thenReturn(null);
        when(mNonUrlTab.getUrl()).thenReturn(CHROME_SCHEME_URL);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.getTabUngrouper()).thenReturn(mTabUngrouper);
        when(mProfile.isOffTheRecord()).thenReturn(true);
        mSavedTabGroup.collaborationId = COLLABORATION_ID;
        mOnItemClickedCallback =
                TabContextMenuCoordinator.getMenuItemClickedCallback(
                        () -> mTabModel,
                        mTabGroupModelFilter,
                        mBottomSheetCoordinator,
                        () -> mShareDelegate);
        mTabContextMenuCoordinator =
                TabContextMenuCoordinator.createContextMenuCoordinator(
                        () -> mTabModel,
                        mTabGroupModelFilter,
                        mBottomSheetCoordinator,
                        () -> mShareDelegate,
                        mWindowAndroid);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testListMenuItems_tabInGroup() {
        var modelList = new ModelList();
        mTabContextMenuCoordinator.buildMenuActionItems(modelList, TAB_ID);

        assertEquals("Number of items in the list menu is incorrect", 4, modelList.size());

        // List item 1
        assertEquals(
                R.string.add_tab_to_group,
                modelList.get(0).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.add_to_tab_group,
                modelList.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 2
        assertEquals(
                R.string.remove_tab_from_group,
                modelList.get(1).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.remove_from_tab_group,
                modelList.get(1).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 3
        assertEquals(R.string.share, modelList.get(2).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.share_tab, modelList.get(2).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 4
        assertEquals(
                R.string.close_tab, modelList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testListMenuItems_tabOutsideOfGroup() {
        var modelList = new ModelList();
        mTabContextMenuCoordinator.buildMenuActionItems(modelList, TAB_OUTSIDE_OF_GROUP_ID);

        assertEquals("Number of items in the list menu is incorrect", 3, modelList.size());

        // List item 1
        assertEquals(
                R.string.add_tab_to_group,
                modelList.get(0).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.add_to_tab_group,
                modelList.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 2
        assertEquals(R.string.share, modelList.get(1).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.share_tab, modelList.get(1).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 3
        assertEquals(
                R.string.close_tab, modelList.get(2).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(2).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testListMenuItems_nonShareableUrl() {
        var modelList = new ModelList();
        mTabContextMenuCoordinator.buildMenuActionItems(modelList, NON_URL_TAB_ID);

        assertEquals("Number of items in the list menu is incorrect", 2, modelList.size());

        // List item 1
        assertEquals(
                R.string.add_tab_to_group,
                modelList.get(0).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.add_to_tab_group,
                modelList.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 2
        assertEquals(
                R.string.close_tab, modelList.get(1).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(1).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testRemoveFromGroup() {
        mOnItemClickedCallback.onClick(R.id.remove_from_tab_group, TAB_ID, COLLABORATION_ID);
        verify(mTabUngrouper, times(1)).ungroupTabs(List.of(mTab1), true, true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testShareUrl() {
        mOnItemClickedCallback.onClick(R.id.share_tab, TAB_ID, COLLABORATION_ID);
        verify(mShareDelegate, times(1)).share(mTab1, false, TAB_STRIP_CONTEXT_MENU);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testCloseTab() {
        mOnItemClickedCallback.onClick(R.id.close_tab, TAB_ID, COLLABORATION_ID);
        verify(mTabRemover, times(1)).closeTabs(TabClosureParams.closeTab(mTab1).build(), true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testAddToTabGroup_newTabGroup() {
        mOnItemClickedCallback.onClick(R.id.add_to_tab_group, TAB_ID, COLLABORATION_ID);
        verify(mBottomSheetCoordinator, times(1)).showBottomSheet(List.of(mTab1));
    }
}
