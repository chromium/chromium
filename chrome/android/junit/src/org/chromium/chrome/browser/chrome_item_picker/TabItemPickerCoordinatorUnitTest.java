// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.chrome_item_picker;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Context;
import android.os.Looper;
import android.view.ViewGroup;
import android.view.WindowManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowDisplay;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.page_content_annotations.PageContentExtractionService;
import org.chromium.chrome.browser.page_content_annotations.PageContentExtractionServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorItemSelectionId;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for TabItemPickerCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabItemPickerCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private Profile mProfile;
    @Mock private TabModelSelectorImpl mTabModelSelector;
    @Mock private Callback<TabModelSelector> mCallback;
    @Mock private Activity mActivity;
    @Mock private TabListEditorCoordinator mTabListEditorCoordinator;
    @Mock private ViewGroup mRootView;
    @Mock private ViewGroup mContainerView;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private TabListEditorCoordinator.TabListEditorController mTabListEditorController;
    @Mock private android.view.Window mWindow;
    @Mock private android.view.ViewGroup mDecorView;
    @Mock private android.content.res.Resources mResources;
    @Mock private WindowManager mWindowManager;
    @Mock private TabModel mRegularTabModel;
    @Mock private PageContentExtractionService mPageContentExtractionService;
    @Mock private WebContents mWebContents;

    @Mock private SelectionDelegate<TabListEditorItemSelectionId> mSelectionDelegateMock;
    @Mock private ObservableSupplier<Boolean> mBackPressChangedSupplierMock;
    @Captor private ArgumentCaptor<List<Tab>> mTabListCaptor;
    private OneshotSupplierImpl<Profile> mProfileSupplierImpl;
    private TabItemPickerCoordinator mItemPickerCoordinator;
    private final int mWindowId = 5;

    @Before
    public void setUp() {
        mProfileSupplierImpl = new OneshotSupplierImpl<>();
        TabItemPickerCoordinator realCoordinator =
                new TabItemPickerCoordinator(
                        mProfileSupplierImpl,
                        mWindowId,
                        mActivity,
                        mSnackbarManager,
                        mRootView,
                        mContainerView,
                        new ArrayList<Integer>(),
                        TabListEditorCoordinator.UNLIMITED_SELECTION);
        mItemPickerCoordinator = Mockito.spy(realCoordinator);

        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);

        when(mActivity.getWindow()).thenReturn(mWindow);
        when(mWindow.getDecorView()).thenReturn(mDecorView);

        when(mActivity.getResources()).thenReturn(mResources);
        when(mResources.getInteger(anyInt())).thenReturn(1);

        when(mActivity.getSystemService(Context.WINDOW_SERVICE)).thenReturn(mWindowManager);
        when(mWindowManager.getDefaultDisplay()).thenReturn(ShadowDisplay.getDefaultDisplay());

        when(mTabModelSelector.getModel(false)).thenReturn(mRegularTabModel);
        when(mRegularTabModel.index()).thenReturn(0);

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabListEditorCoordinator.getController()).thenReturn(mTabListEditorController);
        doReturn(mTabListEditorCoordinator)
                .when(mItemPickerCoordinator)
                .createTabListEditorCoordinator(any(TabModelSelector.class));

        when(mTabListEditorCoordinator.getSelectionDelegate()).thenReturn(mSelectionDelegateMock);
        when(mTabListEditorController.getHandleBackPressChangedSupplier())
                .thenReturn(mBackPressChangedSupplierMock);

        PageContentExtractionServiceFactory.setForTesting(mPageContentExtractionService);
    }

    @After
    public void tearDown() {
        TabWindowManagerSingleton.setTabWindowManagerForTesting(null);
    }

    private Tab mockTabActiveState(int tabId, boolean isActive) {
        Tab tab = Mockito.mock(Tab.class);
        when(tab.getId()).thenReturn(tabId);
        when(mTabModelSelector.getTabById(tabId)).thenReturn(tab);
        when(tab.isFrozen()).thenReturn(!isActive);
        when(tab.isInitialized()).thenReturn(isActive);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

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

    @Test
    public void testShowTabItemPicker_SuccessPath() {
        // Mock the window manager to return a valid selector upon request
        when(TabWindowManagerSingleton.getInstance()
                        .requestSelectorWithoutActivity(anyInt(), any(Profile.class)))
                .thenReturn(mTabModelSelector);

        mItemPickerCoordinator.showTabItemPicker(mCallback);
        mProfileSupplierImpl.set(mProfile);
        shadowOf(Looper.getMainLooper()).idle();

        // Verification 1: The window manager was called correctly
        verify(TabWindowManagerSingleton.getInstance())
                .requestSelectorWithoutActivity(eq(mWindowId), eq(mProfile));

        // Verification 2: The final callback is called with the initialized selector
        verify(mCallback).onResult(mTabModelSelector);
    }

    @Test
    public void testShowTabItemPicker_InvalidWindowIdFailsEarly() {
        // Mock coordinator with an invalid window ID
        TabItemPickerCoordinator coordinatorWithInvalidId =
                new TabItemPickerCoordinator(
                        mProfileSupplierImpl,
                        TabWindowManager.INVALID_WINDOW_ID,
                        mActivity,
                        mSnackbarManager,
                        mRootView,
                        mContainerView,
                        new ArrayList<Integer>(),
                        TabListEditorCoordinator.UNLIMITED_SELECTION);

        coordinatorWithInvalidId.showTabItemPicker(mCallback);
        mProfileSupplierImpl.set(mProfile);
        shadowOf(Looper.getMainLooper()).idle();

        // Verification 1: The window manager should never have been called.
        verify(mTabWindowManager, never())
                .requestSelectorWithoutActivity(anyInt(), any(Profile.class));

        // Verification 2: The final callback is called with null.
        verify(mCallback).onResult(null);
    }

    @Test
    public void testShowTabItemPicker_AcquisitionFailsReturnsNull() {
        // Mock the window manager to explicitly return NULL
        Mockito.when(mTabWindowManager.requestSelectorWithoutActivity(anyInt(), any(Profile.class)))
                .thenReturn(null);

        mItemPickerCoordinator.showTabItemPicker(mCallback);
        mProfileSupplierImpl.set(mProfile);
        shadowOf(Looper.getMainLooper()).idle();

        // Verification 1: The window manager was called (it ran the core logic)
        verify(mTabWindowManager).requestSelectorWithoutActivity(eq(mWindowId), eq(mProfile));

        // Verification 2: The final callback is called with null.
        verify(mCallback).onResult(null);
    }

    @Test
    public void testOnCachedTabIdsRetrieved_filtersAndShowsTabs() {
        // Mock the window manager to return a valid selector upon request
        when(TabWindowManagerSingleton.getInstance()
                        .requestSelectorWithoutActivity(anyInt(), any(Profile.class)))
                .thenReturn(mTabModelSelector);

        // 1. Setup tabs
        GURL testUrl = JUnitTestGURLs.URL_1;
        Tab tab1WithWebContents = Mockito.mock(Tab.class);
        when(tab1WithWebContents.getId()).thenReturn(101);
        when(tab1WithWebContents.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.isLoading()).thenReturn(false);
        when(mWebContents.getRenderWidgetHostView())
                .thenReturn(Mockito.mock(RenderWidgetHostView.class));
        when(tab1WithWebContents.isInitialized()).thenReturn(true);
        when(tab1WithWebContents.isFrozen()).thenReturn(false);
        when(tab1WithWebContents.getUrl()).thenReturn(testUrl);

        Tab tab2NoWebContentsCached = Mockito.mock(Tab.class);
        when(tab2NoWebContentsCached.getId()).thenReturn(102);
        when(tab2NoWebContentsCached.getWebContents()).thenReturn(null);
        when(tab2NoWebContentsCached.isInitialized()).thenReturn(true);
        when(tab2NoWebContentsCached.isFrozen()).thenReturn(false);
        when(tab2NoWebContentsCached.getUrl()).thenReturn(testUrl);

        Tab tab3NoWebContentsNotCached = Mockito.mock(Tab.class);
        when(tab3NoWebContentsNotCached.getId()).thenReturn(103);
        when(tab3NoWebContentsNotCached.getWebContents()).thenReturn(null);
        when(tab3NoWebContentsNotCached.isInitialized()).thenReturn(true);
        when(tab3NoWebContentsNotCached.isFrozen()).thenReturn(false);
        when(tab3NoWebContentsNotCached.getUrl()).thenReturn(testUrl);

        List<Tab> allTabs =
                Arrays.asList(
                        tab1WithWebContents, tab2NoWebContentsCached, tab3NoWebContentsNotCached);
        // Mocking TabModel to return the list of tabs
        when(mRegularTabModel.getCount()).thenReturn(allTabs.size());
        when(mRegularTabModel.getTabAt(0)).thenReturn(tab1WithWebContents);
        when(mRegularTabModel.getTabAt(1)).thenReturn(tab2NoWebContentsCached);
        when(mRegularTabModel.getTabAt(2)).thenReturn(tab3NoWebContentsNotCached);
        when(mRegularTabModel.iterator()).thenReturn(allTabs.iterator());

        // When getAllCachedTabIds is called, immediately call the callback with our test data.
        doAnswer(
                        invocation -> {
                            Callback<long[]> callback = invocation.getArgument(0);
                            callback.onResult(new long[] {102L});
                            return null;
                        })
                .when(mPageContentExtractionService)
                .getAllCachedTabIds(any());

        // 2. Trigger the whole flow
        mItemPickerCoordinator.showTabItemPicker(mCallback);
        mProfileSupplierImpl.set(mProfile);
        shadowOf(Looper.getMainLooper()).idle();

        // 3. Verify the correct tabs are passed to the editor
        verify(mTabListEditorController).show(mTabListCaptor.capture(), any(), any());
        List<Tab> shownTabs = mTabListCaptor.getValue();

        assertEquals("Should show 2 tabs", 2, shownTabs.size());
        assertTrue("Should contain tab with WebContents", shownTabs.contains(tab1WithWebContents));
        assertTrue(
                "Should contain cached tab without WebContents",
                shownTabs.contains(tab2NoWebContentsCached));
        assertFalse(
                "Should not contain non-cached tab without WebContents",
                shownTabs.contains(tab3NoWebContentsNotCached));
    }

    @Test
    public void testOnCachedTabIdsRetrieved_RecordsHistograms() {
        when(TabWindowManagerSingleton.getInstance()
                        .requestSelectorWithoutActivity(anyInt(), any(Profile.class)))
                .thenReturn(mTabModelSelector);
        mProfileSupplierImpl.set(mProfile);

        Tab tabActive1 = mockTabActiveState(1, true);
        Tab tabActive2 = mockTabActiveState(2, true);
        Tab tabCached = mockTabActiveState(3, false);
        Tab tabNeither = mockTabActiveState(4, false);

        List<Tab> allTabs = Arrays.asList(tabActive1, tabActive2, tabCached, tabNeither);
        when(mRegularTabModel.getCount()).thenReturn(allTabs.size());

        when(mRegularTabModel.getTabAt(0)).thenReturn(tabActive1);
        when(mRegularTabModel.getTabAt(1)).thenReturn(tabActive2);
        when(mRegularTabModel.getTabAt(2)).thenReturn(tabCached);
        when(mRegularTabModel.getTabAt(3)).thenReturn(tabNeither);
        when(mRegularTabModel.iterator()).thenReturn(allTabs.iterator());
        when(mTabModelSelector.getModel(false)).thenReturn(mRegularTabModel);

        long[] cachedIdsInput = new long[] {3L};

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.TabItemPicker.ActiveTabs.Count", 2)
                        .expectIntRecord("Android.TabItemPicker.CachedTabs.Count", 1)
                        .build();

        doAnswer(
                        invocation -> {
                            Callback<long[]> callback = invocation.getArgument(0);
                            callback.onResult(cachedIdsInput);
                            return null;
                        })
                .when(mPageContentExtractionService)
                .getAllCachedTabIds(any());

        mItemPickerCoordinator.showTabItemPicker(mCallback);
        shadowOf(Looper.getMainLooper()).idle();

        // Verify expected histogram and that the UI list contains the expected 3 tabs (Active or
        // Cached)
        watcher.assertExpected();
        verify(mTabListEditorController).show(mTabListCaptor.capture(), any(), any());
    }
}
