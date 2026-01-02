// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.chrome_item_picker;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.chrome_item_picker.TabItemPickerCoordinator.ItemPickerNavigationProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorItemSelectionId;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Integration tests for TabItemPickerCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabItemPickerCoordinatorNavigationUnitTest {
    private static final int WINDOW_ID = 5;
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabModelSelectorImpl mTabModelSelector;
    @Mock private ChromeItemPickerActivity mActivity;
    @Mock private TabListEditorCoordinator mTabListEditorCoordinator;
    @Mock private TabListEditorController mTabListEditorController;
    @Mock private SelectionDelegate<TabListEditorItemSelectionId> mSelectionDelegateMock;
    private TabItemPickerCoordinator mItemPickerCoordinator;
    private ItemPickerNavigationProvider mNavigationProvider;
    private Set<Integer> mCachedTabIds = new HashSet<>();

    @Before
    public void setUp() {
        OneshotSupplierImpl<Profile> profileSupplierImpl = new OneshotSupplierImpl<>();
        ViewGroup rootView = Mockito.mock(ViewGroup.class);
        ViewGroup containerView = Mockito.mock(ViewGroup.class);
        SnackbarManager snackbarManager = Mockito.mock(SnackbarManager.class);
        TabItemPickerCoordinator realCoordinator =
                new TabItemPickerCoordinator(
                        profileSupplierImpl,
                        WINDOW_ID,
                        mActivity,
                        snackbarManager,
                        rootView,
                        containerView,
                        new ArrayList<Integer>(),
                        TabListEditorCoordinator.UNLIMITED_SELECTION);
        mItemPickerCoordinator = Mockito.spy(realCoordinator);
        mCachedTabIds = new HashSet<>();
    }

    private Tab mockTabActiveState(int tabId, boolean isActive) {
        Tab tab = Mockito.mock(Tab.class);
        when(tab.getId()).thenReturn(tabId);
        when(mTabModelSelector.getTabById(tabId)).thenReturn(tab);
        when(tab.isFrozen()).thenReturn(!isActive);
        when(tab.isInitialized()).thenReturn(isActive);
        if (isActive) {
            WebContents webContents = Mockito.mock(WebContents.class);
            RenderWidgetHostView rwhv = Mockito.mock(RenderWidgetHostView.class);
            when(tab.getWebContents()).thenReturn(webContents);
            when(webContents.getRenderWidgetHostView()).thenReturn(rwhv);
        } else {
            when(tab.getWebContents()).thenReturn(null);
        }
        return tab;
    }

    private void captureAndSpyNavigationProvider() {
        assumeNonNull(mTabModelSelector);
        mNavigationProvider =
                new ItemPickerNavigationProvider(
                        mActivity,
                        mTabListEditorController,
                        mSelectionDelegateMock,
                        mTabModelSelector,
                        mCachedTabIds);

        doReturn(mNavigationProvider)
                .when(mItemPickerCoordinator)
                .getItemPickerNavigationProviderForTesting();

        doReturn(mTabListEditorCoordinator)
                .when(mItemPickerCoordinator)
                .createTabListEditorCoordinator(any(TabModelSelector.class));

        mItemPickerCoordinator.createTabListEditorCoordinator(mTabModelSelector);
    }

    @Test
    public void testGoBackTriggersCancel() {
        captureAndSpyNavigationProvider();
        when(mTabListEditorController.isVisible()).thenReturn(true);

        mNavigationProvider.goBack();

        InOrder inOrder = inOrder(mTabListEditorController, mActivity);

        inOrder.verify(mTabListEditorController).hide();
        inOrder.verify(mActivity).finishWithCancel();

        verify(mActivity, never()).finish();
    }

    @Test
    public void testFinishSelectionTriggersSuccessWithData() {
        captureAndSpyNavigationProvider();
        // Prepare mock selection IDs.
        TabListEditorItemSelectionId id1 = TabListEditorItemSelectionId.createTabId(101);
        TabListEditorItemSelectionId id2 = TabListEditorItemSelectionId.createTabId(102);
        List<TabListEditorItemSelectionId> selectedList = Arrays.asList(id1, id2);

        // Simulate finishSelection() being called.
        mNavigationProvider.finishSelection(selectedList);

        InOrder inOrder = inOrder(mTabListEditorController, mActivity);

        // Verify the UI hide action is called first.
        inOrder.verify(mTabListEditorController).hideByAction();

        // Verify the Activity success method is called second with the expected data.
        inOrder.verify(mActivity).finishWithSelectedItems(selectedList);
        verify(mActivity, never()).finish();
    }

    @Test
    public void testFinishSelectionRecordsCorrectMetrics() {
        mCachedTabIds.add(102);
        Tab activeTab = mockTabActiveState(101, true);
        Tab cachedTab = mockTabActiveState(102, false);

        captureAndSpyNavigationProvider();

        TabListEditorItemSelectionId id1 = TabListEditorItemSelectionId.createTabId(101);
        TabListEditorItemSelectionId id2 = TabListEditorItemSelectionId.createTabId(102);
        List<TabListEditorItemSelectionId> selectedList = Arrays.asList(id1, id2);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.TabItemPicker.SelectedTabs.Count", 2)
                        .expectIntRecord("Android.TabItemPicker.ActiveTabsPicked.Count", 1)
                        .expectIntRecord("Android.TabItemPicker.CachedTabsPicked.Count", 1)
                        .build();

        mNavigationProvider.finishSelection(selectedList);

        watcher.assertExpected();
        verify(mActivity).finishWithSelectedItems(selectedList);
    }
}
