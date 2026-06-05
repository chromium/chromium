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
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.activity_recreation.ActivityRecreationController.ACTIVITY_RECREATION_UI_STATE;
import static org.chromium.chrome.browser.ui.activity_recreation.ActivityRecreationController.IS_TAB_SWITCHER_SHOWN;
import static org.chromium.chrome.browser.ui.activity_recreation.ActivityRecreationController.URL_BAR_EDIT_TEXT;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.PersistableBundle;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.ExclusiveAccessManager;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.components.omnibox.TextSelection;
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
    @Mock private Tab mActivityTab;
    @Mock private WebContents mWebContents;
    @Mock private ContentView mContentView;
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private Bundle mSavedInstanceState;
    @Mock private ExclusiveAccessManager mExclusiveAccessManager;
    @Captor private ArgumentCaptor<LayoutStateObserver> mLayoutStateObserverCaptor;
    @Captor private ArgumentCaptor<Runnable> mRunnableCaptor;
    @Captor private ArgumentCaptor<AutocompleteInput> mAutocompleteInputCaptor;

    private final ActivityTabProvider mActivityTabProvider = new ActivityTabProvider();
    private ActivityRecreationController mActivityRecreationController;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        ViewAndroidDelegate viewAndroidDelegate =
                ViewAndroidDelegate.createBasicDelegate(mContentView);
        KeyboardVisibilityDelegate.setInstanceForTesting(mKeyboardVisibilityDelegate);
        mActivityTabProvider.setForTesting(mActivityTab);

        doNothing().when(mToolbarManager).beginFuseboxInput(any());
        doNothing().when(mLayoutManager).addObserver(any());
        doReturn(true).when(mLayoutManager).isLayoutStartingToShow(LayoutType.BROWSING);
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
        verify(mLayoutManager).addObserver(mLayoutStateObserverCaptor.capture());

        // Simulate invocation of Layout#doneShowing after invocation of #restoreUiState.
        doReturn(true).when(mLayoutManager).isLayoutVisible(LayoutType.BROWSING);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.BROWSING);
        verify(mHandler).post(mRunnableCaptor.capture());
        mRunnableCaptor.getValue().run();
        verify(mToolbarManager).beginFuseboxInput(mAutocompleteInputCaptor.capture());
        AutocompleteInput input = mAutocompleteInputCaptor.getValue();
        assertEquals(text, input.getUserText());
        assertEquals(OmniboxFocusReason.ACTIVITY_RECREATION_RESTORATION, input.getFocusReason());
        assertEquals(TextSelection.SELECT_ALL, input.getSelection());
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
        verify(mToolbarManager).beginFuseboxInput(mAutocompleteInputCaptor.capture());
        AutocompleteInput input = mAutocompleteInputCaptor.getValue();
        assertEquals(text, input.getUserText());
        assertEquals(OmniboxFocusReason.ACTIVITY_RECREATION_RESTORATION, input.getFocusReason());
        assertEquals(TextSelection.SELECT_ALL, input.getSelection());
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
        verify(mToolbarManager, never()).beginFuseboxInput(any());
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
        verify(mToolbarManager, never()).beginFuseboxInput(any());
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

    @Test
    public void testRestoreOmniboxState_layoutPendingShow() {
        String text = "editText";
        PersistableBundle persistableBundle = new PersistableBundle();
        persistableBundle.putString(URL_BAR_EDIT_TEXT, text);

        mActivityRecreationController.restorePersistentState(persistableBundle);

        verify(mLayoutManager).addObserver(mLayoutStateObserverCaptor.capture());
        // Simulate invocation of Layout#doneShowing after invocation of #restoreOmniboxState.
        doReturn(true).when(mLayoutManager).isLayoutVisible(LayoutType.BROWSING);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.BROWSING);
        verify(mHandler).post(mRunnableCaptor.capture());
        mRunnableCaptor.getValue().run();
        verify(mToolbarManager).beginFuseboxInput(mAutocompleteInputCaptor.capture());
        AutocompleteInput input = mAutocompleteInputCaptor.getValue();
        assertEquals(text, input.getUserText());
        assertEquals(OmniboxFocusReason.ACTIVITY_RECREATION_RESTORATION, input.getFocusReason());
        assertEquals(TextSelection.SELECT_ALL, input.getSelection());
    }

    @Test
    public void testRestoreOmniboxState_layoutDoneShowing() {
        // Assume that Layout#doneShowing is invoked before invocation of #restoreUiState.
        doReturn(true).when(mLayoutManager).isLayoutVisible(LayoutType.BROWSING);
        doReturn(false).when(mLayoutManager).isLayoutStartingToShow(LayoutType.BROWSING);

        String text = "editText";
        PersistableBundle persistableBundle = new PersistableBundle();
        persistableBundle.putString(URL_BAR_EDIT_TEXT, text);

        mActivityRecreationController.restorePersistentState(persistableBundle);
        verify(mToolbarManager).beginFuseboxInput(mAutocompleteInputCaptor.capture());
        assertEquals(text, mAutocompleteInputCaptor.getValue().getUserText());
        assertEquals(
                OmniboxFocusReason.ACTIVITY_RECREATION_RESTORATION,
                mAutocompleteInputCaptor.getValue().getFocusReason());
        assertEquals(TextSelection.SELECT_ALL, mAutocompleteInputCaptor.getValue().getSelection());

        String newText = "newEditText";
        persistableBundle.putString(URL_BAR_EDIT_TEXT, newText);
        mActivityRecreationController.restorePersistentState(persistableBundle);
        verify(mToolbarManager, times(2)).beginFuseboxInput(mAutocompleteInputCaptor.capture());
        assertEquals(newText, mAutocompleteInputCaptor.getValue().getUserText());
        assertEquals(
                OmniboxFocusReason.ACTIVITY_RECREATION_RESTORATION,
                mAutocompleteInputCaptor.getValue().getFocusReason());
        assertEquals(TextSelection.SELECT_ALL, mAutocompleteInputCaptor.getValue().getSelection());

        // Omnibox code should restore keyboard.
        verify(mKeyboardVisibilityDelegate, never()).showKeyboard(mContentView);
    }

    @Test
    public void testRestoreTabSwitcherState_tabSwitcherShown() {
        PersistableBundle persistableBundle = new PersistableBundle();
        persistableBundle.putBoolean(IS_TAB_SWITCHER_SHOWN, true);

        mActivityRecreationController.restorePersistentState(persistableBundle);
        verify(mLayoutManager).showLayout(LayoutType.TAB_SWITCHER, false);
    }

    @Test
    public void testRestoreTabSwitcherState_tabSwitcherNotShown() {
        PersistableBundle persistableBundle = new PersistableBundle();
        persistableBundle.putBoolean(IS_TAB_SWITCHER_SHOWN, false);

        mActivityRecreationController.restorePersistentState(persistableBundle);
        verify(mLayoutManager, never()).showLayout(anyInt(), anyBoolean());
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
        SettableMonotonicObservableSupplier<LayoutManager> layoutManagerSupplier =
                ObservableSuppliers.createMonotonic();
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
