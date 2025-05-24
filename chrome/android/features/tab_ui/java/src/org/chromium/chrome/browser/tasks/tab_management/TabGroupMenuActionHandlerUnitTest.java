// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.annotation.Nullable;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.TabGroupCreationCallback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Unit tests for {@link TabGroupMenuActionHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupMenuActionHandlerUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupModelFilter mFilter;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Profile mProfile;
    @Mock private Tab mTab;
    @Mock private TabGroupListBottomSheetCoordinator mTabGroupListBottomSheetCoordinator;
    @Mock private TabModel mTabModel;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;

    private TabGroupMenuActionHandler mHandler;
    @Nullable private TabGroupCreationCallback mTabGroupCreationCallback;

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        TabGroupSyncServiceFactory.setForTesting(mock());
        CollaborationServiceFactory.setForTesting(mock());
        DataSharingServiceFactory.setForTesting(mock());
        TrackerFactory.setTrackerForTests(mock());
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        doReturn(true).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfile);

        TabGroupListBottomSheetCoordinatorFactory factory =
                (a, b, callback, d, e, f, g, h) -> {
                    mTabGroupCreationCallback = callback;
                    return mTabGroupListBottomSheetCoordinator;
                };

        mHandler =
                new TabGroupMenuActionHandler(
                        context,
                        mFilter,
                        mBottomSheetController,
                        mModalDialogManager,
                        mProfile,
                        factory);
        when(mTab.getTabGroupId()).thenReturn(Token.createRandom());
        when(mFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
    }

    @Test
    public void testHandleAddToGroupAction_noGroups() {
        when(mFilter.getTabGroupCount()).thenReturn(0);
        mHandler.handleAddToGroupAction(mTab);
        verify(mFilter).createSingleTabGroup(mTab);
        verify(mTabGroupListBottomSheetCoordinator, never()).showBottomSheet(any());
    }

    @Test
    public void testHandleAddToGroupAction_withGroups() {
        when(mFilter.getTabGroupCount()).thenReturn(1);
        mHandler.handleAddToGroupAction(mTab);
        verify(mFilter, never()).createSingleTabGroup(mTab);
        verify(mTabGroupListBottomSheetCoordinator).showBottomSheet(any());
    }

    @Test
    public void testOnTabGroupCreation_withCoordinator() {
        when(mFilter.getTabGroupCount()).thenReturn(1);
        mHandler.handleAddToGroupAction(mTab);

        assertNotNull(mTabGroupCreationCallback);
        mTabGroupCreationCallback.onTabGroupCreated(mTab.getTabGroupId());
        verify(mFilter, never()).createSingleTabGroup(mTab);
    }

    @Test
    public void testOnTabGroupCreation_noCoordinator() {
        when(mFilter.getTabGroupCount()).thenReturn(0);
        mHandler.handleAddToGroupAction(mTab);

        verify(mFilter).createSingleTabGroup(mTab);
    }
}
