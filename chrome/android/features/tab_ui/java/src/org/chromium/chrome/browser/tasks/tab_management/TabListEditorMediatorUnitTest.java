// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.CreationMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.ItemPickerSelectionHandler;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.NavigationProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.ResetHandler;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate.SelectionObserver;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.Set;

/** Unit tests for {@link TabListEditorMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class TabListEditorMediatorUnitTest {
    private static final String SYNC_ID = "sync_id_test_guid";
    private static final TabListEditorItemSelectionId TAB_ID_1 =
            TabListEditorItemSelectionId.createTabId(1);
    private static final TabListEditorItemSelectionId TAB_ID_2 =
            TabListEditorItemSelectionId.createTabId(2);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private SelectionDelegate<TabListEditorItemSelectionId> mSelectionDelegate;
    @Mock private TabListCoordinator mTabListCoordinator;
    @Mock private ResetHandler mResetHandler;
    @Mock private TabListEditorLayout mTabListEditorLayout;
    @Mock private TabListEditorToolbar mTabListEditorToolbar;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private Profile mProfile;
    @Mock private NavigationProvider mNavigationProvider;
    @Mock private SnackbarManager mSnackbarManager;

    private final SettableNonNullObservableSupplier<Boolean> mEnableDoneButtonSupplier =
            ObservableSuppliers.createNonNull(false);
    private final ItemPickerSelectionHandler mItemPickerSelectionHandler =
            new ItemPickerSelectionHandler() {
                @Override
                public NonNullObservableSupplier<Boolean> getEnableDoneButtonSupplier() {
                    return mEnableDoneButtonSupplier;
                }

                @Override
                public void onSelectionStateChange(
                        Set<TabListEditorItemSelectionId> selectedItems) {
                    mEnableDoneButtonSupplier.set(
                            !Objects.equals(mInitialSelectedItems, selectedItems));
                }

                @Override
                public void finishSelection(List<TabListEditorItemSelectionId> selectedItems) {}
            };

    private Context mContext;
    private PropertyModel mModel;
    private TabListEditorMediator mMediator;
    private MonotonicObservableSupplier<TabGroupModelFilter> mTabGroupModelFilterSupplier;
    private ArgumentCaptor<SelectionObserver<TabListEditorItemSelectionId>>
            mSelectionObserverCaptor;
    private Set<TabListEditorItemSelectionId> mInitialSelectedItems;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mTabGroupModelFilterSupplier = ObservableSuppliers.createNonNull(mTabGroupModelFilter);

        when(mTabModel.isIncognito()).thenReturn(false);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mTabListEditorLayout.getToolbar()).thenReturn(mTabListEditorToolbar);
        mModel = new PropertyModel.Builder(TabListEditorProperties.ALL_KEYS).build();
        mSelectionObserverCaptor = ArgumentCaptor.forClass(SelectionObserver.class);

        setupMediator(CreationMode.FULL_SCREEN);
    }

    private void setupMediator(@CreationMode int mode) {
        if (mMediator != null) {
            mMediator.destroy();
        }
        mModel = new PropertyModel.Builder(TabListEditorProperties.ALL_KEYS).build();
        reset(mSelectionDelegate);

        ItemPickerSelectionHandler itemPickerSelectionHandler =
                mode == CreationMode.ITEM_PICKER ? mItemPickerSelectionHandler : null;

        mMediator =
                new TabListEditorMediator(
                        mContext,
                        mTabGroupModelFilterSupplier,
                        mModel,
                        mSelectionDelegate,
                        /* actionOnRelatedTabs= */ false,
                        mSnackbarManager,
                        /* bottomSheetController= */ null,
                        mTabListEditorLayout,
                        TabActionState.SELECTABLE,
                        mDesktopWindowStateManager,
                        mode,
                        itemPickerSelectionHandler);
        mMediator.initializeWithTabListCoordinator(mTabListCoordinator, mResetHandler);
        mMediator.setNavigationProvider(mNavigationProvider);
        mSelectionObserverCaptor = ArgumentCaptor.forClass(SelectionObserver.class);

        // Verify times(1) is correct because we reset the mock first.
        verify(mSelectionDelegate, times(1)).addObserver(mSelectionObserverCaptor.capture());
    }

    private void triggerSelection(Set<TabListEditorItemSelectionId> selectedItems) {
        mSelectionObserverCaptor.getValue().onSelectionStateChange(List.copyOf(selectedItems));
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
    public void testConfigureToolbar_NonItemPickerMode() {
        // Test in the default FULL_SCREEN mode.
        setupMediator(CreationMode.FULL_SCREEN);
        mMediator.show(new ArrayList<>(), new ArrayList<>(), /* recyclerViewPosition= */ null);

        assertFalse(mModel.get(TabListEditorProperties.DONE_BUTTON_VISIBILITY));
        assertFalse(mModel.get(TabListEditorProperties.IS_DONE_BUTTON_ENABLED));
    }

    @Test
    public void testItemPicker_DifferentSelection() {
        setupMediator(CreationMode.ITEM_PICKER);
        mMediator.show(new ArrayList<>(), new ArrayList<>(), /* recyclerViewPosition= */ null);

        assertTrue(mModel.get(TabListEditorProperties.DONE_BUTTON_VISIBILITY));
        assertFalse(mModel.get(TabListEditorProperties.IS_DONE_BUTTON_ENABLED));

        // Initial selection contains a tab.
        mInitialSelectedItems = Set.of(TAB_ID_1);
        mMediator.selectTabs(mInitialSelectedItems);

        // Mock current selection contains different tab.
        Set<TabListEditorItemSelectionId> currentSelection = Set.of(TAB_ID_2);
        when(mSelectionDelegate.getSelectedItems()).thenReturn(currentSelection);

        triggerSelection(currentSelection);

        assertTrue(mModel.get(TabListEditorProperties.DONE_BUTTON_VISIBILITY));
        assertTrue(mModel.get(TabListEditorProperties.IS_DONE_BUTTON_ENABLED));
    }

    @Test
    public void testSelection_SingleContext_CheckmarkSync() {
        setupMediator(CreationMode.ITEM_PICKER);

        PropertyModel model1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, 1)
                        .with(TabProperties.IS_SELECTED, true)
                        .build();
        PropertyModel model2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, 2)
                        .with(TabProperties.IS_SELECTED, false)
                        .build();

        TabListModel modelList = new TabListModel();
        modelList.add(new MVCListAdapter.ListItem(TabProperties.UiType.TAB, model1));
        modelList.add(new MVCListAdapter.ListItem(TabProperties.UiType.TAB, model2));
        when(mTabListCoordinator.getTabListModel()).thenReturn(modelList);

        triggerSelection(Set.of(TAB_ID_2));

        assertFalse(model1.get(TabProperties.IS_SELECTED));
        assertTrue(model2.get(TabProperties.IS_SELECTED));
    }
}
