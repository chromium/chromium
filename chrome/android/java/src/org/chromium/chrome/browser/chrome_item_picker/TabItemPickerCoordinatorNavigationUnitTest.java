// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.chrome_item_picker;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Bitmap;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.actor.OffscreenRenderingManager;
import org.chromium.chrome.browser.chrome_item_picker.TabItemPickerCoordinator.ItemPickerNavigationProvider;
import org.chromium.chrome.browser.chrome_item_picker.TabItemPickerCoordinator.ThumbnailCaptureResult;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.utilities.TabLoadingService;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorItemSelectionId;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Integration tests for TabItemPickerCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabItemPickerCoordinatorNavigationUnitTest {
    private static final int WINDOW_ID = 5;

    // cancel_load_on_deselection defaults to false, so tests that exercise cancellation opt in.
    private static final String OPTIMIZATION_CANCEL_ON_DESELECTION =
            ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION
                    + ":cancel_load_on_deselection/true";
    private static final String OPTIMIZATION_CANCEL_AND_FIRST_PAINT =
            OPTIMIZATION_CANCEL_ON_DESELECTION + "/enable_first_paint/true";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabModelSelectorImpl mTabModelSelector;
    @Mock private ChromeItemPickerActivity mActivity;
    @Mock private TabListEditorCoordinator mTabListEditorCoordinator;
    @Mock private TabListEditorController mTabListEditorController;
    @Mock private TabContentManager mTabContentManager;
    @Mock private OffscreenRenderingManager mOffscreenRenderingManager;
    @Mock private WebContents mWebContents;
    @Mock private RenderWidgetHostView mRenderWidgetHostView;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mCallbackCaptor;

    private final Set<TabListEditorItemSelectionId> mInitialSelectedTabIds = new HashSet<>();

    private TabItemPickerCoordinator mItemPickerCoordinator;
    private ItemPickerNavigationProvider mNavigationProvider;
    private Set<Integer> mCachedTabIds = new HashSet<>();

    @Before
    public void setUp() {
        TabLoadingService.getInstance().clearForTesting();
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
                        TabListEditorCoordinator.UNLIMITED_SELECTION,
                        false);
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
                        ObservableSuppliers.createMonotonic(mTabListEditorController),
                        mTabModelSelector,
                        mTabContentManager,
                        mCachedTabIds,
                        mInitialSelectedTabIds,
                        mItemPickerCoordinator::cancelPicker);
        mItemPickerCoordinator.setNavigationProviderForTesting(mNavigationProvider);

        doReturn(mTabListEditorCoordinator)
                .when(mItemPickerCoordinator)
                .createTabListEditorCoordinator(any(TabModelSelector.class));

        mItemPickerCoordinator.createTabListEditorCoordinator(mTabModelSelector);
    }

    @Test
    public void testGoBackTriggersCancel() {
        captureAndSpyNavigationProvider();
        when(mTabListEditorController.isVisible()).thenReturn(true);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.TabItemPicker.Cancel.SelectedTabs.Count", 0)
                        .build();

        mNavigationProvider.goBack();

        watcher.assertExpected();

        InOrder inOrder = inOrder(mTabListEditorController, mActivity);

        inOrder.verify(mTabListEditorController).hide();
        inOrder.verify(mActivity).finishWithCancel();

        verify(mActivity, never()).finish();
    }

    @Test
    public void testGoBackRecordsCancelSelectedTabsCountHistogram_NonEmptySelection() {
        captureAndSpyNavigationProvider();
        when(mTabListEditorController.isVisible()).thenReturn(true);

        TabListEditorItemSelectionId id1 = TabListEditorItemSelectionId.createTabId(101);
        TabListEditorItemSelectionId id2 = TabListEditorItemSelectionId.createTabId(102);
        Set<TabListEditorItemSelectionId> selection = new HashSet<>(Arrays.asList(id1, id2));
        mNavigationProvider.onSelectionStateChange(selection);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.TabItemPicker.Cancel.SelectedTabs.Count", 2)
                        .build();

        mNavigationProvider.goBack();

        watcher.assertExpected();
        verify(mActivity).finishWithCancel();
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
        mockTabActiveState(101, true);
        mockTabActiveState(102, false);

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

    @Test
    public void testSelectionChangeUpdatesDoneButton() {
        TabListEditorItemSelectionId id1 = TabListEditorItemSelectionId.createTabId(101);
        mInitialSelectedTabIds.add(id1);

        captureAndSpyNavigationProvider();

        NonNullObservableSupplier<Boolean> supplier =
                mNavigationProvider.getEnableDoneButtonSupplier();
        assertFalse(supplier.get());

        // Select the same item.
        Set<TabListEditorItemSelectionId> selection = new HashSet<>();
        selection.add(id1);
        mNavigationProvider.onSelectionStateChange(selection);
        assertFalse(supplier.get());

        // Select a different item.
        TabListEditorItemSelectionId id2 = TabListEditorItemSelectionId.createTabId(102);
        selection.add(id2);
        mNavigationProvider.onSelectionStateChange(selection);
        assertTrue(supplier.get());

        // Back to original.
        selection.remove(id2);
        mNavigationProvider.onSelectionStateChange(selection);
        assertFalse(supplier.get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE)
    public void testSelectionChangeLoadsBackgroundTabs() {
        int tabId = 101;
        Tab tab = mockTabActiveState(tabId, false);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(tab.loadIfNeeded(anyBoolean())).thenReturn(true);
        when(tab.isLoading()).thenReturn(true);

        captureAndSpyNavigationProvider();

        TabListEditorItemSelectionId id = TabListEditorItemSelectionId.createTabId(tabId);
        Set<TabListEditorItemSelectionId> selection = new HashSet<>();
        selection.add(id);

        mNavigationProvider.onSelectionStateChange(selection);

        verify(tab).loadIfNeeded(/* forceBackingSize= */ true);
        verify(mTabListEditorController).setThumbnailSpinnerVisibility(tab, true);
        verify(tab).addObserver(mTabObserverCaptor.capture());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE)
    public void testTabLoadFinished() {
        int tabId = 101;
        Tab tab = mockTabActiveState(tabId, false);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(tab.loadIfNeeded(anyBoolean())).thenReturn(true);
        when(tab.isLoading()).thenReturn(true);

        captureAndSpyNavigationProvider();

        TabListEditorItemSelectionId id = TabListEditorItemSelectionId.createTabId(tabId);
        Set<TabListEditorItemSelectionId> selection = new HashSet<>();
        selection.add(id);

        mNavigationProvider.onSelectionStateChange(selection);

        verify(tab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Android.TabItemPicker.OnDemandLoadDuration.Success")
                        .build();

        observer.onPageLoadFinished(tab, JUnitTestGURLs.URL_1);

        watcher.assertExpected();
        verify(tab).removeObserver(observer);
        verify(mTabContentManager)
                .cacheTabThumbnailWithCallback(eq(tab), eq(true), mCallbackCaptor.capture());

        var thumbnailWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Android.TabItemPicker.OnDemandThumbnailFetchDuration")
                        .expectIntRecord(
                                "Android.TabItemPicker.ThumbnailCaptureResult",
                                ThumbnailCaptureResult.CAPTURE_EMPTY)
                        .build();

        mCallbackCaptor.getValue().onResult(null);

        thumbnailWatcher.assertExpected();
        verify(mTabListEditorController).setThumbnailSpinnerVisibility(tab, false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE)
    public void testTabLoadFailed() {
        int tabId = 101;
        Tab tab = mockTabActiveState(tabId, false);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(tab.loadIfNeeded(anyBoolean())).thenReturn(true);
        when(tab.isLoading()).thenReturn(true);

        captureAndSpyNavigationProvider();

        TabListEditorItemSelectionId id = TabListEditorItemSelectionId.createTabId(tabId);
        Set<TabListEditorItemSelectionId> selection = new HashSet<>();
        selection.add(id);

        mNavigationProvider.onSelectionStateChange(selection);

        verify(tab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Android.TabItemPicker.OnDemandLoadDuration.Failure")
                        .build();

        observer.onPageLoadFailed(tab, 0);

        watcher.assertExpected();
        verify(tab).removeObserver(observer);
        verify(mTabContentManager, never())
                .cacheTabThumbnailWithCallback(any(), anyBoolean(), any());
        verify(mTabListEditorController).setThumbnailSpinnerVisibility(tab, false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE)
    public void testTabLoadFinished_AlreadyLoaded() {
        int tabId = 101;
        Tab tab = mockTabActiveState(tabId, false);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(tab.loadIfNeeded(anyBoolean())).thenReturn(true);
        when(tab.isLoading()).thenReturn(false);

        captureAndSpyNavigationProvider();

        TabListEditorItemSelectionId id = TabListEditorItemSelectionId.createTabId(tabId);
        Set<TabListEditorItemSelectionId> selection = new HashSet<>();
        selection.add(id);

        mNavigationProvider.onSelectionStateChange(selection);

        verify(tab, never()).addObserver(any());
        verify(mTabContentManager, never())
                .cacheTabThumbnailWithCallback(any(), anyBoolean(), any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE)
    public void testSelectionChangeDoesNotLoadActiveTabs() {
        int tabId = 101;
        Tab tab = mockTabActiveState(tabId, true);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

        captureAndSpyNavigationProvider();

        TabListEditorItemSelectionId id = TabListEditorItemSelectionId.createTabId(tabId);
        Set<TabListEditorItemSelectionId> selection = new HashSet<>();
        selection.add(id);

        mNavigationProvider.onSelectionStateChange(selection);

        verify(tab, never()).loadIfNeeded(anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE)
    public void testSelectionChangeLoadsBackgroundTabs_RedundantTrigger() {
        int tabId = 101;
        Tab tab = mockTabActiveState(tabId, false);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(tab.loadIfNeeded(anyBoolean())).thenReturn(true);
        when(tab.isLoading()).thenReturn(true);

        captureAndSpyNavigationProvider();

        TabListEditorItemSelectionId id = TabListEditorItemSelectionId.createTabId(tabId);
        Set<TabListEditorItemSelectionId> selection = new HashSet<>();
        selection.add(id);

        mNavigationProvider.onSelectionStateChange(selection);
        mNavigationProvider.onSelectionStateChange(selection);

        verify(tab, times(1)).loadIfNeeded(anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE)
    public void testSelectionChangeDoesNotAddObserverTwice() {
        int tabId = 101;
        Tab tab = mockTabActiveState(tabId, false);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(tab.loadIfNeeded(anyBoolean())).thenReturn(true);
        when(tab.isLoading()).thenReturn(true);

        captureAndSpyNavigationProvider();

        TabListEditorItemSelectionId id = TabListEditorItemSelectionId.createTabId(tabId);
        Set<TabListEditorItemSelectionId> selection = new HashSet<>();
        selection.add(id);

        mNavigationProvider.onSelectionStateChange(selection);
        mNavigationProvider.onSelectionStateChange(selection);

        verify(tab, times(1)).addObserver(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE)
    public void testSelectionChangeDoesNotShowSpinnerTwice() {
        int tabId = 101;
        Tab tab = mockTabActiveState(tabId, false);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(tab.loadIfNeeded(anyBoolean())).thenReturn(true);
        when(tab.isLoading()).thenReturn(true);

        captureAndSpyNavigationProvider();

        TabListEditorItemSelectionId id = TabListEditorItemSelectionId.createTabId(tabId);
        Set<TabListEditorItemSelectionId> selection = new HashSet<>();
        selection.add(id);

        mNavigationProvider.onSelectionStateChange(selection);
        mNavigationProvider.onSelectionStateChange(selection);

        verify(mTabListEditorController, times(1)).setThumbnailSpinnerVisibility(tab, true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE)
    public void testSelectionChange_RedundantTrigger_Loading() {
        int tabId = 101;
        Tab tab = mockTabActiveState(tabId, false);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(tab.loadIfNeeded(anyBoolean())).thenReturn(true);
        when(tab.isLoading()).thenReturn(true);

        captureAndSpyNavigationProvider();

        TabListEditorItemSelectionId id = TabListEditorItemSelectionId.createTabId(tabId);
        Set<TabListEditorItemSelectionId> selection = new HashSet<>();
        selection.add(id);

        mNavigationProvider.onSelectionStateChange(selection);
        mNavigationProvider.onSelectionStateChange(selection);

        verify(tab, times(1)).loadIfNeeded(anyBoolean());
        // cacheTabThumbnailWithCallback is not called yet because the tab is still loading.
        verify(mTabContentManager, never())
                .cacheTabThumbnailWithCallback(any(), anyBoolean(), any());
        // Spinner is shown once.
        verify(mTabListEditorController, times(1)).setThumbnailSpinnerVisibility(tab, true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE)
    public void testSelectionChangeDoesNotLoadCachedTabs() {
        int tabId = 101;
        mCachedTabIds.add(tabId);
        Tab tab = mockTabActiveState(tabId, false);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

        captureAndSpyNavigationProvider();

        TabListEditorItemSelectionId id = TabListEditorItemSelectionId.createTabId(tabId);
        Set<TabListEditorItemSelectionId> selection = new HashSet<>();
        selection.add(id);

        mNavigationProvider.onSelectionStateChange(selection);

        verify(tab, never()).loadIfNeeded(anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE)
    public void testSelectionChangeDoesNotLoadIneligibleTabs() {
        int tabId = 101;
        Tab tab = mockTabActiveState(tabId, false);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.CHROME_ABOUT);

        captureAndSpyNavigationProvider();

        TabListEditorItemSelectionId id = TabListEditorItemSelectionId.createTabId(tabId);
        Set<TabListEditorItemSelectionId> selection = new HashSet<>();
        selection.add(id);

        mNavigationProvider.onSelectionStateChange(selection);

        verify(tab, never()).loadIfNeeded(anyBoolean());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE)
    public void testSelectionChangeDoesNotLoadWhenFeatureDisabled() {
        int tabId = 101;
        Tab tab = mockTabActiveState(tabId, false);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

        captureAndSpyNavigationProvider();

        TabListEditorItemSelectionId id = TabListEditorItemSelectionId.createTabId(tabId);
        Set<TabListEditorItemSelectionId> selection = new HashSet<>();
        selection.add(id);

        mNavigationProvider.onSelectionStateChange(selection);

        verify(tab, never()).loadIfNeeded(anyBoolean());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE,
        OPTIMIZATION_CANCEL_AND_FIRST_PAINT
    })
    public void testOffscreenRendering_StartedOnSelectionAndStoppedOnCaptureComplete() {
        OffscreenRenderingManager mockOffscreenManager =
                Mockito.mock(OffscreenRenderingManager.class);
        OffscreenRenderingManager.setInstanceForTesting(mockOffscreenManager);

        int tabId = 101;
        Tab tab = mockTabActiveState(tabId, false);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(tab.loadIfNeeded(anyBoolean())).thenReturn(true);
        when(tab.isLoading()).thenReturn(true);

        WebContents webContents = Mockito.mock(WebContents.class);
        when(tab.getWebContents()).thenReturn(webContents);
        when(webContents.isDestroyed()).thenReturn(false);

        captureAndSpyNavigationProvider();

        TabListEditorItemSelectionId id = TabListEditorItemSelectionId.createTabId(tabId);
        Set<TabListEditorItemSelectionId> selection = new HashSet<>();
        selection.add(id);

        mNavigationProvider.onSelectionStateChange(selection);

        verify(mockOffscreenManager).startOffscreenRendering(eq(tab), anyInt(), anyInt());

        verify(tab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        observer.didFirstVisuallyNonEmptyPaint(tab);

        verify(mTabContentManager)
                .cacheTabThumbnailWithCallback(eq(tab), eq(true), mCallbackCaptor.capture());
        verify(mockOffscreenManager, never()).stopOffscreenRendering(tab);

        mCallbackCaptor.getValue().onResult(null);
        verify(mockOffscreenManager).stopOffscreenRendering(tab);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE,
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION
    })
    public void testOffscreenRendering_StoppedOnLoadFailure() {
        OffscreenRenderingManager mockOffscreenManager =
                Mockito.mock(OffscreenRenderingManager.class);
        OffscreenRenderingManager.setInstanceForTesting(mockOffscreenManager);

        int tabId = 101;
        Tab tab = mockTabActiveState(tabId, false);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(tab.loadIfNeeded(anyBoolean())).thenReturn(true);
        when(tab.isLoading()).thenReturn(true);

        WebContents webContents = Mockito.mock(WebContents.class);
        when(tab.getWebContents()).thenReturn(webContents);
        when(webContents.isDestroyed()).thenReturn(false);

        captureAndSpyNavigationProvider();

        TabListEditorItemSelectionId id = TabListEditorItemSelectionId.createTabId(tabId);
        Set<TabListEditorItemSelectionId> selection = new HashSet<>();
        selection.add(id);

        mNavigationProvider.onSelectionStateChange(selection);

        verify(mockOffscreenManager).startOffscreenRendering(eq(tab), anyInt(), anyInt());

        verify(tab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        observer.onPageLoadFailed(tab, 500);

        verify(mockOffscreenManager).stopOffscreenRendering(tab);
        verify(mTabContentManager, never())
                .cacheTabThumbnailWithCallback(any(), anyBoolean(), any());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE,
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION
    })
    public void testOffscreenRendering_CleanedUpOnDestroy() {
        OffscreenRenderingManager mockOffscreenManager =
                Mockito.mock(OffscreenRenderingManager.class);
        OffscreenRenderingManager.setInstanceForTesting(mockOffscreenManager);

        int tabId = 101;
        Tab tab = mockTabActiveState(tabId, false);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(tab.loadIfNeeded(anyBoolean())).thenReturn(true);
        when(tab.isLoading()).thenReturn(true);

        WebContents webContents = Mockito.mock(WebContents.class);
        when(tab.getWebContents()).thenReturn(webContents);
        when(webContents.isDestroyed()).thenReturn(false);

        captureAndSpyNavigationProvider();

        TabListEditorItemSelectionId id = TabListEditorItemSelectionId.createTabId(tabId);
        Set<TabListEditorItemSelectionId> selection = new HashSet<>();
        selection.add(id);

        mNavigationProvider.onSelectionStateChange(selection);

        verify(mockOffscreenManager).startOffscreenRendering(eq(tab), anyInt(), anyInt());

        verify(tab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        // Load finishes and thumbnail capture is in flight.
        observer.didFirstVisuallyNonEmptyPaint(tab);
        verify(mockOffscreenManager, never()).stopOffscreenRendering(tab);

        // Picker destroyed while thumbnail capture is in flight.
        mNavigationProvider.destroy();

        verify(mockOffscreenManager).stopOffscreenRendering(tab);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE,
        OPTIMIZATION_CANCEL_ON_DESELECTION
    })
    public void testSelectionKeepsExistingThumbnail() {
        selectLoadableTab(101);

        verify(mTabContentManager, never()).removeTabThumbnail(anyInt(), anyBoolean());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE,
        OPTIMIZATION_CANCEL_ON_DESELECTION
    })
    public void testDeselectionKeepsExistingThumbnail() {
        Tab tab = selectLoadableTab(101);

        mNavigationProvider.onSelectionStateChange(new HashSet<>());

        verify(tab).stopLoading();
        verify(mTabContentManager, never()).removeTabThumbnail(anyInt(), anyBoolean());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE,
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION
                + ":cancel_load_on_deselection/false"
    })
    public void testDeselectionDoesNotCancelWhenParamDisabled() {
        Tab tab = selectLoadableTab(101);
        clearInvocations(tab);

        mNavigationProvider.onSelectionStateChange(new HashSet<>());

        verify(tab, never()).stopLoading();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE)
    @DisableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION)
    public void testDeselectionDoesNotCancelWhenOptimizationDisabled() {
        Tab tab = selectLoadableTab(101);
        clearInvocations(tab);

        mNavigationProvider.onSelectionStateChange(new HashSet<>());

        verify(tab, never()).stopLoading();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE)
    @DisableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION)
    public void testSelectionClearsThumbnailWhenOptimizationDisabled() {
        selectLoadableTab(101);

        verify(mTabContentManager).removeTabThumbnail(101, /* forceRemoval= */ true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE)
    @DisableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION)
    public void testReselectionSkipsActiveTabWhenOptimizationDisabled() {
        Tab tab = selectLoadableTab(101);
        verify(tab).addObserver(mTabObserverCaptor.capture());
        mTabObserverCaptor.getValue().onPageLoadFinished(tab, JUnitTestGURLs.URL_1);
        simulateCancelledRendererState(tab);
        clearInvocations(tab);

        mNavigationProvider.onSelectionStateChange(selectionFor(101));

        verify(tab, never()).loadIfNeeded(anyBoolean());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE,
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION
    })
    public void testLoadFailureRemovesStaleThumbnail() {
        Tab tab = selectLoadableTab(101);
        verify(tab).addObserver(mTabObserverCaptor.capture());

        mTabObserverCaptor.getValue().onPageLoadFailed(tab, 500);

        verify(mTabContentManager).removeTabThumbnail(101, /* forceRemoval= */ true);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE,
        OPTIMIZATION_CANCEL_ON_DESELECTION
    })
    public void testReselectionAfterCancelReloadsTab() {
        Tab tab = selectLoadableTab(101);
        mNavigationProvider.onSelectionStateChange(new HashSet<>());
        simulateCancelledRendererState(tab);
        clearInvocations(tab);

        mNavigationProvider.onSelectionStateChange(selectionFor(101));

        verify(tab).loadIfNeeded(anyBoolean());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE,
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION
    })
    public void testReselectionSkipsTabWithLoadedContent() {
        Tab tab = selectLoadableTab(101);
        mNavigationProvider.onSelectionStateChange(new HashSet<>());
        simulateCancelledRendererState(tab);
        when(tab.needsReload()).thenReturn(false);
        clearInvocations(tab);

        mNavigationProvider.onSelectionStateChange(selectionFor(101));

        verify(tab, never()).loadIfNeeded(anyBoolean());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE,
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION
    })
    public void testDestroyKeepsSelectedTabLoading() {
        Tab tab = selectLoadableTab(101);
        clearInvocations(tab);

        mNavigationProvider.destroy();

        verify(tab, never()).stopLoading();
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE,
        OPTIMIZATION_CANCEL_ON_DESELECTION
    })
    public void testFinishSelection_RecordsCancelledLoadsCount() {
        Tab tab = selectLoadableTab(101);
        mNavigationProvider.onSelectionStateChange(Collections.emptySet());

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.TabItemPicker.CancelledLoads.Count", 1)
                        .expectIntRecord("Android.TabItemPicker.NetCancelledTabs.Count", 1)
                        .expectBooleanRecord("Android.TabItemPicker.CancelledTabReselected", false)
                        .build();

        mNavigationProvider.finishSelection(Collections.emptyList());

        watcher.assertExpected();
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE,
        OPTIMIZATION_CANCEL_ON_DESELECTION
    })
    public void testCancelPicker_RecordsCancelCancelledLoadsCount() {
        Tab tab = selectLoadableTab(101);
        when(mTabListEditorController.isVisible()).thenReturn(true);
        mNavigationProvider.onSelectionStateChange(Collections.emptySet());

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.TabItemPicker.Cancel.CancelledLoads.Count", 1)
                        .expectIntRecord("Android.TabItemPicker.Cancel.NetCancelledTabs.Count", 1)
                        .expectBooleanRecord("Android.TabItemPicker.CancelledTabReselected", false)
                        .build();

        mNavigationProvider.goBack();

        watcher.assertExpected();
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE,
        OPTIMIZATION_CANCEL_ON_DESELECTION
    })
    public void testReselectionAfterCancellation_GrossVsNetMetrics() {
        Tab tab = selectLoadableTab(101);
        mNavigationProvider.onSelectionStateChange(Collections.emptySet());
        simulateCancelledRendererState(tab);

        // Re-select tab 101.
        mNavigationProvider.onSelectionStateChange(selectionFor(101));

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.TabItemPicker.CancelledLoads.Count", 1)
                        .expectIntRecord("Android.TabItemPicker.NetCancelledTabs.Count", 0)
                        .expectBooleanRecord("Android.TabItemPicker.CancelledTabReselected", true)
                        .build();

        mNavigationProvider.finishSelection(
                Arrays.asList(TabListEditorItemSelectionId.createTabId(101)));

        watcher.assertExpected();
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE,
        OPTIMIZATION_CANCEL_ON_DESELECTION
    })
    public void testFinishSelection_NoCancellations_DoesNotRecordCancelledTabReselected() {
        selectLoadableTab(101);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.TabItemPicker.CancelledTabReselected")
                        .build();

        mNavigationProvider.finishSelection(
                Arrays.asList(TabListEditorItemSelectionId.createTabId(101)));

        watcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE)
    public void testThumbnailCaptureResult_LiveCaptured() {
        Tab tab = selectLoadableTab(101);
        verify(tab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        observer.onPageLoadFinished(tab, JUnitTestGURLs.URL_1);
        verify(mTabContentManager)
                .cacheTabThumbnailWithCallback(eq(tab), eq(true), mCallbackCaptor.capture());

        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.TabItemPicker.ThumbnailCaptureResult",
                                ThumbnailCaptureResult.LIVE_CAPTURED)
                        .build();

        mCallbackCaptor.getValue().onResult(bitmap);

        watcher.assertExpected();
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE,
        OPTIMIZATION_CANCEL_ON_DESELECTION
    })
    public void testThumbnailCaptureResult_CancelledPreserved() {
        selectLoadableTab(101);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.TabItemPicker.ThumbnailCaptureResult",
                                ThumbnailCaptureResult.CANCELLED_PRESERVED)
                        .build();

        mNavigationProvider.onSelectionStateChange(Collections.emptySet());

        watcher.assertExpected();
    }

    /**
     * Simulates the state a tab is left in after its on-demand load is cancelled: the renderer is
     * still alive, but the content must be reloaded.
     */
    private void simulateCancelledRendererState(Tab tab) {
        when(tab.isInitialized()).thenReturn(true);
        when(tab.isFrozen()).thenReturn(false);
        when(tab.needsReload()).thenReturn(true);
        when(mWebContents.getRenderWidgetHostView()).thenReturn(mRenderWidgetHostView);
    }

    private static Set<TabListEditorItemSelectionId> selectionFor(int... tabIds) {
        Set<TabListEditorItemSelectionId> selection = new HashSet<>();
        for (int tabId : tabIds) {
            selection.add(TabListEditorItemSelectionId.createTabId(tabId));
        }
        return selection;
    }

    /** Mocks a frozen, eligible tab and selects it so that a load is started. */
    private Tab selectLoadableTab(int tabId) {
        OffscreenRenderingManager.setInstanceForTesting(mOffscreenRenderingManager);

        Tab tab = mockTabActiveState(tabId, false);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(tab.loadIfNeeded(anyBoolean())).thenReturn(true);
        when(tab.isLoading()).thenReturn(true);

        when(tab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.isDestroyed()).thenReturn(false);

        captureAndSpyNavigationProvider();

        mNavigationProvider.onSelectionStateChange(selectionFor(tabId));
        return tab;
    }
}
