// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.CallbackUtils;
import org.chromium.base.JniRepeatingRunnable;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.EventForwarder;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link CoBrowseViews}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CoBrowseViewsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabBottomSheetWebUi mWebUi;
    @Mock private WebContents mWebContents;
    @Mock private EventForwarder mEventForwarder;
    @Mock private CoBrowseComponentProvider mMockContentProvider;

    private final View mWebUiView = new View(ApplicationProvider.getApplicationContext());
    private final View mPeekView = new View(ApplicationProvider.getApplicationContext());
    private Context mContext;
    private CoBrowseViews mCoBrowseViews;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mWebUi.getWebUiView()).thenReturn(mWebUiView);
        when(mWebContents.getEventForwarder()).thenReturn(mEventForwarder);

        View rootView = LayoutInflater.from(mContext).inflate(R.layout.tab_bottom_sheet, null);
        mCoBrowseViews =
                new CoBrowseViews(
                        rootView,
                        TabBottomSheetClientType.CONTEXTUAL_TASKS,
                        CoBrowseContainerType.BOTTOM_SHEET,
                        mWebUi,
                        Color.WHITE,
                        /* placeholderElemColor= */ 0,
                        null,
                        SupplierUtils.ofNull(),
                        CallbackUtils.emptyRunnable());
    }

    @Test
    public void testConstructor_BuildsViewHierarchy() {
        View view = mCoBrowseViews.getView();
        assertNotNull(view);

        ViewGroup webUiContainer = view.findViewById(R.id.web_ui_container);

        assertEquals(1, webUiContainer.getChildCount());
        assertEquals(mWebUiView, webUiContainer.getChildAt(0));
    }

    @Test
    public void testConstructor_SidePanel_RemovesMargin() {
        // Create a new CoBrowseViews object with the desired container type (SIDE_PANEL).
        View rootView = LayoutInflater.from(mContext).inflate(R.layout.tab_bottom_sheet, null);
        CoBrowseViews coBrowseViews =
                new CoBrowseViews(
                        rootView,
                        TabBottomSheetClientType.CONTEXTUAL_TASKS,
                        CoBrowseContainerType.SIDE_PANEL,
                        mWebUi,
                        Color.WHITE,
                        /* placeholderElemColor= */ 0,
                        mMockContentProvider,
                        SupplierUtils.ofNull(),
                        CallbackUtils.emptyRunnable());

        View view = coBrowseViews.getView();
        ViewGroup webUiContainer = view.findViewById(R.id.web_ui_container);

        assertEquals(0, ((MarginLayoutParams) webUiContainer.getLayoutParams()).topMargin);
    }

    @Test
    public void testDestroy() {
        mCoBrowseViews.destroy();

        verify(mWebUi).destroy();

        View view = mCoBrowseViews.getView();
        ViewGroup webUiContainer = view.findViewById(R.id.web_ui_container);

        assertEquals(0, webUiContainer.getChildCount());
    }

    @Test
    public void testSetWebUiTouchHandler() {
        TabBottomSheetWebUiContainer.TouchHandler handler =
                mock(TabBottomSheetWebUiContainer.TouchHandler.class);
        mCoBrowseViews.setWebUiTouchHandler(handler);
        // Verifies that it doesn't crash.
    }

    @Test
    public void testAttachAndRemovePeekView() {
        mCoBrowseViews.attachPeekView(mPeekView);
        assertTrue(mCoBrowseViews.hasPeekView());

        View view = mCoBrowseViews.getView();
        ViewGroup peekContainer = view.findViewById(R.id.peek_view_container);
        assertEquals(1, peekContainer.getChildCount());
        assertEquals(mPeekView, peekContainer.getChildAt(0));

        mCoBrowseViews.removePeekView(mPeekView);
        assertTrue(!mCoBrowseViews.hasPeekView());
        assertEquals(0, peekContainer.getChildCount());
    }

    @Test
    public void testSetWebContents_withFocus() {
        mCoBrowseViews.setWebContents(mWebContents, true);
        verify(mWebUi).setWebContents(mWebContents, true);
    }

    @Test
    public void testSetWebContents_withoutFocus() {
        mCoBrowseViews.setWebContents(mWebContents, false);
        verify(mWebUi).setWebContents(mWebContents, false);
    }

    @Test
    public void testConstructor_WithContentProvider() {
        View rootView = LayoutInflater.from(mContext).inflate(R.layout.tab_bottom_sheet, null);
        CoBrowseViews coBrowseViews =
                new CoBrowseViews(
                        rootView,
                        TabBottomSheetClientType.CONTEXTUAL_TASKS,
                        CoBrowseContainerType.BOTTOM_SHEET,
                        mWebUi,
                        Color.WHITE,
                        /* placeholderElemColor= */ 0,
                        mMockContentProvider,
                        SupplierUtils.ofNull(),
                        CallbackUtils.emptyRunnable());
        assertEquals(mMockContentProvider, coBrowseViews.getContentProvider());
    }

    @Test
    public void testSetWebContents_UpdatesViewWhenChanged() {
        View newWebUiView = new View(mContext);
        when(mWebUi.getWebUiView()).thenReturn(mWebUiView).thenReturn(newWebUiView);

        mCoBrowseViews.setWebContents(mWebContents, true);

        View view = mCoBrowseViews.getView();
        ViewGroup webUiContainer = view.findViewById(R.id.web_ui_container);
        assertEquals(1, webUiContainer.getChildCount());
        assertEquals(newWebUiView, webUiContainer.getChildAt(0));
    }

    @Test
    public void testPlaceholder_usePlaceholderTrue() {
        when(mMockContentProvider.setupPlaceholderView(any())).thenReturn(true);
        View rootView = LayoutInflater.from(mContext).inflate(R.layout.tab_bottom_sheet, null);
        CoBrowseViews coBrowseViews =
                new CoBrowseViews(
                        rootView,
                        TabBottomSheetClientType.CONTEXTUAL_TASKS,
                        CoBrowseContainerType.BOTTOM_SHEET,
                        mWebUi,
                        Color.WHITE,
                        /* placeholderElemColor= */ 0,
                        mMockContentProvider,
                        SupplierUtils.ofNull(),
                        CallbackUtils.emptyRunnable());
        assertTrue(coBrowseViews.isPlaceholderSetUp());
        verify(mMockContentProvider).setupPlaceholderView(any());
    }

    @Test
    public void testPlaceholder_usePlaceholderFalse() {
        when(mMockContentProvider.setupPlaceholderView(any())).thenReturn(false);
        View rootView = LayoutInflater.from(mContext).inflate(R.layout.tab_bottom_sheet, null);
        CoBrowseViews coBrowseViews =
                new CoBrowseViews(
                        rootView,
                        TabBottomSheetClientType.CONTEXTUAL_TASKS,
                        CoBrowseContainerType.BOTTOM_SHEET,
                        mWebUi,
                        Color.WHITE,
                        /* placeholderElemColor= */ 0,
                        mMockContentProvider,
                        SupplierUtils.ofNull(),
                        CallbackUtils.emptyRunnable());
        assertTrue(!coBrowseViews.isPlaceholderSetUp());
        verify(mMockContentProvider).setupPlaceholderView(any());
    }

    @DisabledTest(message = "crbug.com/525122374")
    @Test
    public void testPlaceholderAllowedSupplier() {
        when(mMockContentProvider.setupPlaceholderView(any())).thenReturn(true);
        View rootView = LayoutInflater.from(mContext).inflate(R.layout.tab_bottom_sheet, null);
        CoBrowseViews coBrowseViews =
                new CoBrowseViews(
                        rootView,
                        TabBottomSheetClientType.CONTEXTUAL_TASKS,
                        CoBrowseContainerType.BOTTOM_SHEET,
                        mWebUi,
                        Color.WHITE,
                        /* placeholderElemColor= */ 0,
                        mMockContentProvider,
                        SupplierUtils.ofNull(),
                        CallbackUtils.emptyRunnable());

        View placeholderView = rootView.findViewById(R.id.empty_placeholder_container);
        assertEquals(View.VISIBLE, placeholderView.getVisibility());

        SettableNullableObservableSupplier<Boolean> supplier =
                ObservableSuppliers.createNullable(false);
        coBrowseViews.setPlaceholderAllowedSupplier(supplier);
        assertEquals(View.GONE, placeholderView.getVisibility());

        supplier.set(true);
        assertEquals(View.VISIBLE, placeholderView.getVisibility());

        coBrowseViews.setPlaceholderAllowedSupplier(null);
        assertEquals(View.VISIBLE, placeholderView.getVisibility());
    }

    @Test
    public void testGetOrCreatePeekViewManager_CachesInstance() {
        PeekViewManager manager = mock(PeekViewManager.class);
        CoBrowseViews coBrowseViews = createCoBrowseViewsWithPeekViewManagers(manager);

        assertEquals(manager, coBrowseViews.getOrCreatePeekViewManager());
        assertEquals(manager, coBrowseViews.getOrCreatePeekViewManager());
    }

    @Test
    public void testDestroyPeekViewManager_DestroysAndClearsCache() {
        PeekViewManager firstManager = mock(PeekViewManager.class);
        PeekViewManager secondManager = mock(PeekViewManager.class);
        CoBrowseViews coBrowseViews =
                createCoBrowseViewsWithPeekViewManagers(firstManager, secondManager);
        assertEquals(firstManager, coBrowseViews.getOrCreatePeekViewManager());

        coBrowseViews.destroyPeekViewManager();

        verify(firstManager).destroy();
        // A destroyed manager has unregistered its observers, so it must not be handed out again.
        assertEquals(secondManager, coBrowseViews.getOrCreatePeekViewManager());
    }

    @Test
    public void testDestroyPeekViewManager_ManagerNeverCreated_IsNoOp() {
        PeekViewManager manager = mock(PeekViewManager.class);
        // getOrCreatePeekViewManager() is deliberately not called, so nothing is cached yet.
        CoBrowseViews coBrowseViews = createCoBrowseViewsWithPeekViewManagers(manager);

        coBrowseViews.destroyPeekViewManager();

        verify(manager, never()).destroy();
    }

    @Test
    public void testDestroy_DestroysPeekViewManager() {
        PeekViewManager manager = mock(PeekViewManager.class);
        CoBrowseViews coBrowseViews = createCoBrowseViewsWithPeekViewManagers(manager);
        assertEquals(manager, coBrowseViews.getOrCreatePeekViewManager());

        coBrowseViews.destroy();

        verify(manager).destroy();
    }

    /**
     * Creates a {@link CoBrowseViews} whose peek view manager supplier returns the given managers,
     * one per call, so that tests can tell a cached instance apart from a freshly created one.
     */
    private CoBrowseViews createCoBrowseViewsWithPeekViewManagers(PeekViewManager... managers) {
        List<PeekViewManager> remaining = new ArrayList<>(Arrays.asList(managers));
        View rootView = LayoutInflater.from(mContext).inflate(R.layout.tab_bottom_sheet, null);
        return new CoBrowseViews(
                rootView,
                TabBottomSheetClientType.CONTEXTUAL_TASKS,
                CoBrowseContainerType.BOTTOM_SHEET,
                mWebUi,
                Color.WHITE,
                /* placeholderElemColor= */ 0,
                null,
                () -> remaining.isEmpty() ? null : remaining.remove(0),
                CallbackUtils.emptyRunnable());
    }

    @Test
    public void testLoadingPlaceholder_MutualExclusionWithEmptyPlaceholderAndWebContents() {
        View loadingPlaceholder = new View(mContext);
        when(mMockContentProvider.setupPlaceholderView(any())).thenReturn(true);
        when(mMockContentProvider.createLoadingPlaceholderView(
                        any(), anyInt(), anyInt(), anyBoolean(), any()))
                .thenReturn(loadingPlaceholder);

        View rootView = LayoutInflater.from(mContext).inflate(R.layout.tab_bottom_sheet, null);
        CoBrowseViews coBrowseViews =
                new CoBrowseViews(
                        rootView,
                        TabBottomSheetClientType.GLIC,
                        CoBrowseContainerType.BOTTOM_SHEET,
                        mWebUi,
                        Color.WHITE,
                        Color.LTGRAY,
                        mMockContentProvider,
                        SupplierUtils.ofNull(),
                        CallbackUtils.emptyRunnable());

        View emptyPlaceholder = rootView.findViewById(R.id.empty_placeholder_container);
        ViewGroup webUiContainer = rootView.findViewById(R.id.web_ui_container);
        ViewGroup loadingContainer = rootView.findViewById(R.id.loading_placeholder_container);
        assertEquals(loadingPlaceholder, coBrowseViews.getLoadingPlaceholderViewForTesting());
        assertEquals(loadingContainer, loadingPlaceholder.getParent());
        assertEquals(View.GONE, loadingContainer.getVisibility());
        assertEquals(View.VISIBLE, webUiContainer.getVisibility());

        // Allow empty placeholder while WebContents is null: empty placeholder is visible.
        coBrowseViews.setPlaceholderAllowedSupplier(ObservableSuppliers.createNullable(true));
        assertEquals(View.VISIBLE, emptyPlaceholder.getVisibility());
        assertEquals(View.GONE, loadingContainer.getVisibility());
        assertEquals(View.VISIBLE, webUiContainer.getVisibility());

        // Entering loading state shows loading placeholder, hides web_ui_container, and suppresses
        // empty placeholder.
        coBrowseViews.setIsLoading(true);
        assertEquals(View.VISIBLE, loadingContainer.getVisibility());
        assertEquals(View.INVISIBLE, webUiContainer.getVisibility());
        assertEquals(View.GONE, emptyPlaceholder.getVisibility());

        // Attaching non-null WebContents in <webview> mode while still loading keeps loading
        // placeholder visible and web_ui_container hidden behind it.
        coBrowseViews.setWebContents(mWebContents, false);
        assertEquals(View.VISIBLE, loadingContainer.getVisibility());
        assertEquals(View.INVISIBLE, webUiContainer.getVisibility());
        assertEquals(View.GONE, emptyPlaceholder.getVisibility());

        // Leaving loading state while WebContents is attached reveals web_ui_container and keeps
        // empty placeholder hidden.
        coBrowseViews.setIsLoading(false);
        assertEquals(View.GONE, loadingContainer.getVisibility());
        assertEquals(View.VISIBLE, webUiContainer.getVisibility());
        assertEquals(View.GONE, emptyPlaceholder.getVisibility());

        // Clearing WebContents while not loading restores empty placeholder.
        coBrowseViews.setWebContents(null, false);
        assertEquals(View.GONE, loadingContainer.getVisibility());
        assertEquals(View.VISIBLE, webUiContainer.getVisibility());
        assertEquals(View.VISIBLE, emptyPlaceholder.getVisibility());

        // Bottom-sheet loading container matches web_ui_container's non-zero top margin.
        assertEquals(
                ((MarginLayoutParams) webUiContainer.getLayoutParams()).topMargin,
                ((MarginLayoutParams) loadingContainer.getLayoutParams()).topMargin);

        // Destroying detaches loading placeholder and restores web_ui_container visibility.
        coBrowseViews.setIsLoading(true);
        coBrowseViews.destroy();
        assertNull(loadingPlaceholder.getParent());
        assertEquals(View.GONE, loadingContainer.getVisibility());
        assertEquals(View.VISIBLE, webUiContainer.getVisibility());
    }

    @Test
    public void testLoadingPlaceholder_CloseCallbackPassedAndDestroyed() {
        View loadingPlaceholder = new View(mContext);
        final Runnable[] capturedCloseCallback = new Runnable[1];
        when(mMockContentProvider.createLoadingPlaceholderView(
                        any(), anyInt(), anyInt(), anyBoolean(), any()))
                .thenAnswer(
                        invocation -> {
                            capturedCloseCallback[0] = invocation.getArgument(4);
                            return loadingPlaceholder;
                        });

        JniRepeatingRunnable closeRunnable = mock(JniRepeatingRunnable.class);
        View rootView = LayoutInflater.from(mContext).inflate(R.layout.tab_bottom_sheet, null);
        CoBrowseViews coBrowseViews =
                new CoBrowseViews(
                        rootView,
                        TabBottomSheetClientType.GLIC,
                        CoBrowseContainerType.BOTTOM_SHEET,
                        mWebUi,
                        Color.WHITE,
                        Color.LTGRAY,
                        mMockContentProvider,
                        SupplierUtils.ofNull(),
                        closeRunnable);

        assertEquals(closeRunnable, capturedCloseCallback[0]);
        capturedCloseCallback[0].run();
        verify(closeRunnable).run();
        verify(closeRunnable, never()).destroy();

        // destroy() detaches the placeholder and releases the JNI-backed callback.
        coBrowseViews.destroy();
        assertNull(loadingPlaceholder.getParent());
        verify(closeRunnable).destroy();
    }

    @Test
    public void testLoadingPlaceholder_SidePanelZeroTopMargin() {
        View loadingPlaceholder = new View(mContext);
        when(mMockContentProvider.createLoadingPlaceholderView(
                        any(), anyInt(), anyInt(), anyBoolean(), any()))
                .thenReturn(loadingPlaceholder);

        View rootView = LayoutInflater.from(mContext).inflate(R.layout.tab_bottom_sheet, null);
        CoBrowseViews coBrowseViews =
                new CoBrowseViews(
                        rootView,
                        TabBottomSheetClientType.GLIC,
                        CoBrowseContainerType.SIDE_PANEL,
                        mWebUi,
                        Color.WHITE,
                        Color.LTGRAY,
                        mMockContentProvider,
                        SupplierUtils.ofNull(),
                        CallbackUtils.emptyRunnable());

        View loadingContainer = rootView.findViewById(R.id.loading_placeholder_container);
        assertEquals(loadingContainer, loadingPlaceholder.getParent());
        assertEquals(0, ((MarginLayoutParams) loadingContainer.getLayoutParams()).topMargin);
        coBrowseViews.destroy();
    }
}
