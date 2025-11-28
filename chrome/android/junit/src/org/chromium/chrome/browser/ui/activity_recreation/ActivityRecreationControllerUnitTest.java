// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.activity_recreation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.activity_recreation.ActivityRecreationController.ACTIVITY_RECREATION_UI_STATE;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.ExclusiveAccessManager;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ViewAndroidDelegate;

/** Unit tests for {@link ActivityRecreationController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActivityRecreationControllerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private ToolbarManager mToolbarManager;
    @Mock private LayoutManager mLayoutManager;
    @Mock private Handler mHandler;
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private Tab mActivityTab;
    @Mock private WebContents mWebContents;
    @Mock private ContentView mContentView;
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private Bundle mSavedInstanceState;
    @Mock private ExclusiveAccessManager mExclusiveAccessManager;

    private ActivityRecreationController mActivityRecreationController;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        ViewAndroidDelegate viewAndroidDelegate =
                ViewAndroidDelegate.createBasicDelegate(mContentView);
        KeyboardVisibilityDelegate.setInstance(mKeyboardVisibilityDelegate);

        doNothing().when(mToolbarManager).setUrlBarFocusAndText(anyBoolean(), anyInt(), any());
        doNothing().when(mLayoutManager).addObserver(any());
        doReturn(true).when(mLayoutManager).isLayoutStartingToShow(LayoutType.BROWSING);
        doReturn(mActivityTab).when(mActivityTabProvider).get();
        doReturn(context).when(mActivityTab).getContext();
        doReturn(mWebContents).when(mActivityTab).getWebContents();
        doReturn(viewAndroidDelegate).when(mWebContents).getViewAndroidDelegate();
        doNothing().when(mWebContents).scrollFocusedEditableNodeIntoView();
        doNothing().when(mKeyboardVisibilityDelegate).showKeyboard(mContentView);
        doNothing().when(mLayoutManager).showLayout(anyInt(), anyBoolean());

        doReturn(false).when(mToolbarManager).isUrlBarFocused();
        doReturn("").when(mToolbarManager).getUrlBarTextWithoutAutocomplete();

        initializeController();
    }

    @After
    public void tearDown() {
        KeyboardVisibilityDelegate.setInstance(null);
    }

    @Test
    public void testSaveUiState_urlBarFocused() {
        String text = "hello";
        Bundle bundle = new Bundle();
        doReturn(true).when(mToolbarManager).isUrlBarFocused();
        doReturn(text).when(mToolbarManager).getUrlBarTextWithoutAutocomplete();
        mActivityRecreationController.prepareUiState();
        mActivityRecreationController.saveUiState(bundle);

        ActivityRecreationUiState uiState = bundle.getParcelable(ACTIVITY_RECREATION_UI_STATE);
        Assert.assertNotNull("UI state should be saved", uiState);
        assertTrue("Url bar should be focused", uiState.mIsUrlBarFocused);
        assertEquals("Url bar edit text should match", text, uiState.mUrlBarEditText);
    }

    @Test
    public void testSaveUiState_urlBarFocusCleared() {
        String text = "hi";
        String emptyText = "";
        Bundle bundle = new Bundle();
        doReturn(true).when(mToolbarManager).isUrlBarFocused();
        doReturn(text).when(mToolbarManager).getUrlBarTextWithoutAutocomplete();
        mActivityRecreationController.prepareUiState();
        // Remove focus from the URL bar.
        doReturn(false).when(mToolbarManager).isUrlBarFocused();
        doReturn(emptyText).when(mToolbarManager).getUrlBarTextWithoutAutocomplete();
        mActivityRecreationController.saveUiState(bundle);

        ActivityRecreationUiState uiState = bundle.getParcelable(ACTIVITY_RECREATION_UI_STATE);
        Assert.assertNotNull("UI state should be saved", uiState);
        assertTrue("Url bar should be focused", uiState.mIsUrlBarFocused);
        assertEquals("Url bar edit text should match", text, uiState.mUrlBarEditText);
    }

    @Test
    public void testSaveUiState_keyboardVisibleOnWebContentsFocus() {
        Bundle bundle = new Bundle();
        doReturn(true).when(mWebContents).isFocusedElementEditable();
        doReturn(true).when(mKeyboardVisibilityDelegate).isKeyboardShowing(any());
        mActivityRecreationController.prepareUiState();
        mActivityRecreationController.saveUiState(bundle);

        ActivityRecreationUiState uiState = bundle.getParcelable(ACTIVITY_RECREATION_UI_STATE);
        Assert.assertNotNull("UI state should be saved", uiState);
        assertTrue("Soft keyboard should be shown", uiState.mIsKeyboardShown);
        verify(mWebContents).isFocusedElementEditable();
        verify(mKeyboardVisibilityDelegate).isKeyboardShowing(mContentView);
    }

    @Test
    public void testSaveUiState_keyboardVisibleOnWebContentsFocus_crbug1426678() {
        Bundle bundle1 = new Bundle();
        Bundle bundle2 = new Bundle();
        doReturn(true).when(mWebContents).isFocusedElementEditable();
        doReturn(true).when(mKeyboardVisibilityDelegate).isKeyboardShowing(any());
        mActivityRecreationController.prepareUiState();
        mActivityRecreationController.saveUiState(bundle1);

        ActivityRecreationUiState uiState = bundle1.getParcelable(ACTIVITY_RECREATION_UI_STATE);
        Assert.assertNotNull("UI state should be saved", uiState);
        assertTrue("Soft keyboard should be shown", uiState.mIsKeyboardShown);

        // Simulate a second invocation of Activity#onSaveInstanceState.
        doReturn(true).when(mWebContents).isFocusedElementEditable();
        doReturn(false).when(mKeyboardVisibilityDelegate).isKeyboardShowing(any());
        mActivityRecreationController.saveUiState(bundle2);

        uiState = bundle2.getParcelable(ACTIVITY_RECREATION_UI_STATE);
        Assert.assertNotNull("UI state should be saved", uiState);
        assertTrue("Soft keyboard should be shown", uiState.mIsKeyboardShown);

        verify(mWebContents).isFocusedElementEditable();
        verify(mKeyboardVisibilityDelegate).isKeyboardShowing(mContentView);
    }

    @Test
    public void testSaveUiState_tabSwitcherVisible() {
        Bundle bundle = new Bundle();
        doReturn(true).when(mLayoutManager).isLayoutVisible(LayoutType.TAB_SWITCHER);
        mActivityRecreationController.prepareUiState();
        mActivityRecreationController.saveUiState(bundle);
        ActivityRecreationUiState uiState = bundle.getParcelable(ACTIVITY_RECREATION_UI_STATE);
        Assert.assertNotNull("UI state should be saved", uiState);
        assertTrue("Tab switcher should be shown", uiState.mIsTabSwitcherShown);
    }

    @Test
    public void testSaveUiState_defaultValue() {
        Bundle bundle = new Bundle();
        doReturn(false).when(mToolbarManager).isUrlBarFocused();
        doReturn(false).when(mWebContents).isFocusedElementEditable();
        doReturn(false).when(mKeyboardVisibilityDelegate).isKeyboardShowing(any());
        doReturn(false).when(mLayoutManager).isLayoutVisible(LayoutType.TAB_SWITCHER);
        mActivityRecreationController.prepareUiState();
        mActivityRecreationController.saveUiState(bundle);
        ActivityRecreationUiState uiState = bundle.getParcelable(ACTIVITY_RECREATION_UI_STATE);
        Assert.assertNull("UI state should not be saved", uiState);
    }

    @Test
    public void testRestoreUiState_urlBarFocused_layoutPendingShow() {
        String text = "hello";
        initializeSavedInstanceState(
                /* urlBarFocused= */ true,
                text,
                /* keyboardVisible= */ false,
                /* tabSwitcherVisible= */ false,
                /* isPointerLock= */ false,
                /* isKeyboardLock= */ false);
        mActivityRecreationController.restoreUiState(mSavedInstanceState);
        ArgumentCaptor<LayoutStateObserver> layoutStateObserverCaptor =
                ArgumentCaptor.forClass(LayoutStateObserver.class);
        verify(mLayoutManager).addObserver(layoutStateObserverCaptor.capture());

        // Simulate invocation of Layout#doneShowing after invocation of #restoreUiState.
        doReturn(true).when(mLayoutManager).isLayoutVisible(LayoutType.BROWSING);
        layoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.BROWSING);
        ArgumentCaptor<Runnable> postRunnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mHandler).post(postRunnableCaptor.capture());
        postRunnableCaptor.getValue().run();
        verify(mToolbarManager)
                .setUrlBarFocusAndText(
                        true, OmniboxFocusReason.ACTIVITY_RECREATION_RESTORATION, text);
    }

    @Test
    public void testRestoreUiState_urlBarFocused_layoutDoneShowing() {
        String text = "hello";
        // Assume that Layout#doneShowing is invoked before invocation of #restoreUiState.
        doReturn(true).when(mLayoutManager).isLayoutVisible(LayoutType.BROWSING);
        doReturn(false).when(mLayoutManager).isLayoutStartingToShow(LayoutType.BROWSING);
        initializeSavedInstanceState(
                /* urlBarFocused= */ true,
                text,
                /* keyboardVisible= */ true,
                /* tabSwitcherVisible= */ false,
                /* isPointerLock= */ false,
                /* isKeyboardLock= */ false);
        mActivityRecreationController.restoreUiState(mSavedInstanceState);
        verify(mToolbarManager)
                .setUrlBarFocusAndText(
                        true, OmniboxFocusReason.ACTIVITY_RECREATION_RESTORATION, text);
        // Omnibox code should restore keyboard.
        verify(mKeyboardVisibilityDelegate, never()).showKeyboard(mContentView);
    }

    @Test
    public void testRestoreUiState_keyboardVisibleOnWebContentsFocus_layoutDoneShowing() {
        // Assume that Layout#doneShowing is invoked before invocation of #restoreUiState.
        doReturn(true).when(mLayoutManager).isLayoutVisible(LayoutType.BROWSING);
        doReturn(false).when(mLayoutManager).isLayoutStartingToShow(LayoutType.BROWSING);
        initializeSavedInstanceState(
                /* urlBarFocused= */ false,
                null,
                /* keyboardVisible= */ true,
                /* tabSwitcherVisible= */ false,
                /* isPointerLock= */ false,
                /* isKeyboardLock= */ false);
        mActivityRecreationController.restoreUiState(mSavedInstanceState);

        verify(mWebContents).scrollFocusedEditableNodeIntoView();
        verify(mKeyboardVisibilityDelegate).showKeyboard(mContentView);
    }

    @Test
    public void testRestoreUiState_tabSwitcherVisible() {
        initializeSavedInstanceState(
                /* urlBarFocused= */ false,
                null,
                /* keyboardVisible= */ false,
                /* tabSwitcherVisible= */ true,
                /* isPointerLock= */ false,
                /* isKeyboardLock= */ false);
        mActivityRecreationController.restoreUiState(mSavedInstanceState);
        verify(mLayoutManager).showLayout(LayoutType.TAB_SWITCHER, false);
    }

    @Test
    public void testRestoreUiState_urlBarNotFocused() {
        initializeSavedInstanceState(
                /* urlBarFocused= */ false,
                null,
                /* keyboardVisible= */ false,
                /* tabSwitcherVisible= */ false,
                /* isPointerLock= */ false,
                /* isKeyboardLock= */ false);
        mActivityRecreationController.restoreUiState(mSavedInstanceState);
        verify(mLayoutManager, never()).addObserver(any());
        verify(mToolbarManager, never()).setUrlBarFocusAndText(anyBoolean(), anyInt(), any());
    }

    @Test
    public void testRestoreUiState_noStateToRetain() {
        initializeSavedInstanceState(
                /* urlBarFocused= */ false,
                "",
                /* keyboardVisible= */ false,
                /* tabSwitcherVisible= */ false,
                /* isPointerLock= */ false,
                /* isKeyboardLock= */ false);
        mActivityRecreationController.restoreUiState(mSavedInstanceState);
        verify(mLayoutManager, never()).addObserver(any());
        verify(mToolbarManager, never()).setUrlBarFocusAndText(anyBoolean(), anyInt(), any());
        verify(mExclusiveAccessManager, never()).enterFullscreenModeForTab(any(), any());
        verify(mExclusiveAccessManager, never())
                .requestPointerLock(any(), anyBoolean(), anyBoolean());
        verify(mExclusiveAccessManager, never()).requestKeyboardLock(any(), anyBoolean());
    }

    @Test
    public void testRestoreUiState_LocksStateRetain() {
        initializeSavedInstanceState(
                /* urlBarFocused= */ false,
                "",
                /* keyboardVisible= */ false,
                /* tabSwitcherVisible= */ false,
                /* isPointerLock= */ true,
                /* isKeyboardLock= */ true);
        mActivityRecreationController.restoreUiState(mSavedInstanceState);
        verify(mExclusiveAccessManager, never()).enterFullscreenModeForTab(any(), any());
        verify(mExclusiveAccessManager).requestPointerLock(any(), eq(true), eq(true));
        verify(mExclusiveAccessManager).requestKeyboardLock(any(), eq(false));
    }

    private void initializeSavedInstanceState(
            boolean urlBarFocused,
            String urlBarText,
            boolean keyboardVisible,
            boolean tabSwitcherVisible,
            boolean isPointerLock,
            boolean isKeyboardLock) {
        ActivityRecreationUiState uiState =
                new ActivityRecreationUiState(
                        urlBarFocused,
                        urlBarText,
                        keyboardVisible,
                        tabSwitcherVisible,
                        isPointerLock,
                        isKeyboardLock);
        doReturn(uiState).when(mSavedInstanceState).getParcelable(ACTIVITY_RECREATION_UI_STATE);
    }

    private void initializeController() {
        var toolbarManagerSupplier = new OneshotSupplierImpl<ToolbarManager>();
        toolbarManagerSupplier.set(mToolbarManager);
        var layoutManagerSupplier = new ObservableSupplierImpl<LayoutManager>();
        layoutManagerSupplier.set(mLayoutManager);
        mActivityRecreationController =
                new ActivityRecreationController(
                        toolbarManagerSupplier,
                        layoutManagerSupplier,
                        mActivityTabProvider,
                        mHandler,
                        mExclusiveAccessManager);
    }
}
