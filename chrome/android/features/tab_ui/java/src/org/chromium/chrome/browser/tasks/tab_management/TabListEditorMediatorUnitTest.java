// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.CreationMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.ResetHandler;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.Set;

/** Unit tests for {@link TabListEditorMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class TabListEditorMediatorUnitTest {
    private static final String SYNC_ID = "sync_id_test_guid";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private SelectionDelegate<TabListEditorItemSelectionId> mSelectionDelegate;
    @Mock private TabListCoordinator mTabListCoordinator;
    @Mock private ResetHandler mResetHandler;
    @Mock private TabListEditorLayout mTabListEditorLayout;
    @Mock private TabListEditorToolbar mTabListEditorToolbar;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private Profile mProfile;

    private Context mContext;
    private PropertyModel mModel;
    private TabListEditorMediator mMediator;
    private ObservableSupplier<TabGroupModelFilter> mTabGroupModelFilterSupplier;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mTabGroupModelFilterSupplier = new ObservableSupplierImpl<>(mTabGroupModelFilter);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);

        when(mTabModel.isIncognito()).thenReturn(false);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mTabListEditorLayout.getToolbar()).thenReturn(mTabListEditorToolbar);
        mModel = new PropertyModel.Builder(TabListEditorProperties.ALL_KEYS).build();
        mMediator =
                new TabListEditorMediator(
                        mContext,
                        mTabGroupModelFilterSupplier,
                        mModel,
                        mSelectionDelegate,
                        /* actionOnRelatedTabs= */ false,
                        /* snackbarManager= */ null,
                        /* bottomSheetController= */ null,
                        mTabListEditorLayout,
                        TabActionState.SELECTABLE,
                        mDesktopWindowStateManager,
                        CreationMode.FULL_SCREEN);
        mMediator.initializeWithTabListCoordinator(mTabListCoordinator, mResetHandler);
    }

    @After
    public void tearDown() {
        mMediator.destroy();
    }

    @Test
    public void testTopMarginOnAppHeaderStateChange() {
        AppHeaderState state = mock(AppHeaderState.class);
        when(state.getAppHeaderHeight()).thenReturn(10);

        mMediator.onAppHeaderStateChanged(state);

        assertEquals(10, mModel.get(TabListEditorProperties.TOP_MARGIN));
    }

    @Test
    public void testSelectTabs() {
        Set<TabListEditorItemSelectionId> itemIds =
                Set.of(TabListEditorItemSelectionId.createTabId(1));
        mMediator.selectTabs(itemIds);
        verify(mSelectionDelegate).setSelectedItems(itemIds);
        verify(mResetHandler).resetWithListOfTabs(anyList(), eq(null), eq(null), eq(true));
    }

    @Test
    public void testActionOnRelatedTabs_addsSyncedTabGroupTabCount() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        SavedTabGroupTab savedTabGroupTab = new SavedTabGroupTab();
        savedTabGroup.savedTabs = List.of(savedTabGroupTab);

        List<TabListEditorItemSelectionId> itemIds =
                List.of(TabListEditorItemSelectionId.createTabGroupSyncId(SYNC_ID));

        when(mTabGroupSyncService.getGroup(SYNC_ID)).thenReturn(savedTabGroup);

        mMediator =
                new TabListEditorMediator(
                        mContext,
                        mTabGroupModelFilterSupplier,
                        mModel,
                        mSelectionDelegate,
                        /* actionOnRelatedTabs= */ true,
                        /* snackbarManager= */ null,
                        /* bottomSheetController= */ null,
                        mTabListEditorLayout,
                        TabActionState.SELECTABLE,
                        mDesktopWindowStateManager,
                        CreationMode.FULL_SCREEN);
        mMediator.initializeWithTabListCoordinator(mTabListCoordinator, mResetHandler);

        TabListEditorToolbar.RelatedTabCountProvider provider =
                mModel.get(TabListEditorProperties.RELATED_TAB_COUNT_PROVIDER);
        int tabCount = provider.getRelatedTabCount(itemIds);
        assertEquals(savedTabGroup.savedTabs.size(), tabCount);
    }
}
