// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.TabGroupListBottomSheetCoordinatorDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabGroupListBottomSheetCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupListBottomSheetCoordinatorUnitTest {

    private static final Token TAB_GROUP_ID = Token.createRandom();
    private static final String TAB_GROUP_ID_STRING = TAB_GROUP_ID.toString();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private TabGroupModelFilter mFilter;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private Profile mProfile;
    @Mock private Tab mTab;
    private final SavedTabGroup mSavedTabGroup = new SavedTabGroup();
    private final SavedTabGroupTab mSavedTabGroupTab = new SavedTabGroupTab();
    private Context mContext;
    private TabGroupListBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        DataSharingServiceFactory.setForTesting(mDataSharingService);

        mSavedTabGroup.savedTabs = new ArrayList<>(List.of(mSavedTabGroupTab));
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {TAB_GROUP_ID_STRING});
        when(mTabGroupSyncService.getGroup(TAB_GROUP_ID_STRING)).thenReturn(mSavedTabGroup);

        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mCoordinator =
                new TabGroupListBottomSheetCoordinator(
                        mContext,
                        mProfile,
                        /* tabGroupCreationCallback= */ ignored -> {},
                        /* tabMovedCallback= */ null,
                        mFilter,
                        mBottomSheetController,
                        /* supportsShowNewGroup= */ true,
                        /* destroyOnHide= */ false);
    }

    @Test
    public void testRequestShowContent() {
        List<Tab> tabs = new ArrayList<>(List.of(mTab));
        mCoordinator.showBottomSheet(tabs);
        verify(mBottomSheetController)
                .requestShowContent(any(TabGroupListBottomSheetView.class), eq(true));
    }

    @Test
    public void testDelegateRequestShowContent() {
        TabGroupListBottomSheetCoordinatorDelegate delegate = mCoordinator.createDelegate(false);
        delegate.requestShowContent();
        verify(mBottomSheetController)
                .requestShowContent(any(TabGroupListBottomSheetView.class), eq(true));
    }

    @Test
    public void testHide() {
        mCoordinator = spy(mCoordinator);
        TabGroupListBottomSheetCoordinatorDelegate delegate = mCoordinator.createDelegate(false);
        delegate.hide(StateChangeReason.INTERACTION_COMPLETE);
        verify(mBottomSheetController)
                .hideContent(
                        any(TabGroupListBottomSheetView.class),
                        eq(true),
                        eq(StateChangeReason.INTERACTION_COMPLETE));
        verify(mCoordinator, never()).destroy();
    }

    @Test
    public void testDestroyOnHide() {
        mCoordinator = spy(mCoordinator);
        TabGroupListBottomSheetCoordinatorDelegate delegate = mCoordinator.createDelegate(true);
        delegate.hide(StateChangeReason.INTERACTION_COMPLETE);
        verify(mBottomSheetController)
                .hideContent(
                        any(TabGroupListBottomSheetView.class),
                        eq(true),
                        eq(StateChangeReason.INTERACTION_COMPLETE));
        verify(mCoordinator).destroy();
    }

    @Test
    public void testIncognito_dontFetchTabGroupSyncService() {
        when(mProfile.isOffTheRecord()).thenReturn(true);
        // Test that the Coordinator can be constructed without crashing.
        new TabGroupListBottomSheetCoordinator(
                mContext,
                mProfile,
                /* tabGroupCreationCallback= */ ignored -> {},
                /* tabMovedCallback= */ null,
                mFilter,
                mBottomSheetController,
                /* supportsShowNewGroup= */ true,
                /* destroyOnHide= */ false);
    }
}
