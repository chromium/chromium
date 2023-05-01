// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fold_transitions;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.fold_transitions.FoldTransitionController.DID_CHANGE_TABLET_MODE;
import static org.chromium.chrome.browser.ui.fold_transitions.FoldTransitionController.KEYBOARD_VISIBILITY_STATE;
import static org.chromium.chrome.browser.ui.fold_transitions.FoldTransitionController.URL_BAR_EDIT_TEXT;
import static org.chromium.chrome.browser.ui.fold_transitions.FoldTransitionController.URL_BAR_FOCUS_STATE;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.shadows.ShadowSystemClock;

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
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ViewAndroidDelegate;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link FoldTransitionController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FoldTransitionControllerUnitTest {
    @Mock
    private ToolbarManager mToolbarManager;
    @Mock
    private LayoutManager mLayoutManager;
    @Mock
    private Handler mHandler;
    @Mock
    private ActivityTabProvider mActivityTabProvider;
    @Mock
    private Tab mActivityTab;
    @Mock
    private WebContents mWebContents;
    @Mock
    private ContentView mContentView;
    @Mock
    private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;

    private FoldTransitionController mFoldTransitionController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
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
        doReturn(true).when(mToolbarManager).isUrlBarFocused();
        doReturn(text).when(mToolbarManager).getUrlBarTextWithoutAutocomplete();
        Bundle savedInstanceState = new Bundle();
        mFoldTransitionController.saveUiState(savedInstanceState, /* didChangeTabletMode= */ true);

        Assert.assertTrue("Saved instance state should contain URL_BAR_FOCUS_STATE.",
                savedInstanceState.containsKey(URL_BAR_FOCUS_STATE));
        Assert.assertTrue("URL_BAR_FOCUS_STATE in the saved instance state should be true.",
                savedInstanceState.getBoolean(URL_BAR_FOCUS_STATE));
        Assert.assertTrue("Saved instance state should contain URL_BAR_EDIT_TEXT.",
                savedInstanceState.containsKey(URL_BAR_EDIT_TEXT));
        Assert.assertEquals("URL_BAR_EDIT_TEXT in the saved instance state should match.", text,
                savedInstanceState.getString(URL_BAR_EDIT_TEXT));
    }

    @Test
    public void testSaveUiState_urlBarNotFocused() {
        doReturn(false).when(mToolbarManager).isUrlBarFocused();
        Bundle savedInstanceState = new Bundle();
        mFoldTransitionController.saveUiState(savedInstanceState, /* didChangeTabletMode= */ true);

        Assert.assertFalse("Saved instance state should not contain URL_BAR_FOCUS_STATE.",
                savedInstanceState.containsKey(URL_BAR_FOCUS_STATE));
        Assert.assertFalse("Saved instance state should not contain URL_BAR_EDIT_TEXT.",
                savedInstanceState.containsKey(URL_BAR_EDIT_TEXT));
    }

    @Test
    public void testSaveUiState_keyboardVisibleOnWebContentsFocus() {
        doReturn(true).when(mWebContents).isFocusedElementEditable();
        doReturn(true).when(mKeyboardVisibilityDelegate).isKeyboardShowing(any(), any());
        Bundle savedInstanceState = new Bundle();
        mFoldTransitionController.saveUiState(savedInstanceState, /* didChangeTabletMode= */ true);

        Assert.assertTrue("Saved instance state should contain KEYBOARD_VISIBILITY_STATE.",
                savedInstanceState.containsKey(KEYBOARD_VISIBILITY_STATE));
        Assert.assertTrue("KEYBOARD_VISIBILITY_STATE in the saved instance state should be true.",
                savedInstanceState.getBoolean(KEYBOARD_VISIBILITY_STATE));

        verify(mWebContents).isFocusedElementEditable();
        verify(mKeyboardVisibilityDelegate)
                .isKeyboardShowing(mActivityTab.getContext(), mContentView);
    }

    @Test
    public void testSaveUiState_keyboardVisibleOnWebContentsFocus_crbug1426678() {
        doReturn(true).when(mWebContents).isFocusedElementEditable();
        doReturn(true).when(mKeyboardVisibilityDelegate).isKeyboardShowing(any(), any());
        Bundle savedInstanceState = new Bundle();
        mFoldTransitionController.saveUiState(savedInstanceState, /* didChangeTabletMode= */ false);

        Assert.assertTrue("Saved instance state should contain KEYBOARD_VISIBILITY_STATE.",
                savedInstanceState.containsKey(KEYBOARD_VISIBILITY_STATE));
        Assert.assertTrue("KEYBOARD_VISIBILITY_STATE in the saved instance state should be true.",
                savedInstanceState.getBoolean(KEYBOARD_VISIBILITY_STATE));
        Assert.assertTrue("|mKeyboardVisibleDuringFoldTransition| should be true.",
                mFoldTransitionController.getKeyboardVisibleDuringFoldTransitionForTesting());
        Assert.assertNotNull("|mKeyboardVisibilityTimestamp| should not be null.",
                mFoldTransitionController.getKeyboardVisibilityTimestampForTesting());

        ShadowSystemClock.advanceBy(FoldTransitionController.KEYBOARD_RESTORATION_TIMEOUT_MS - 1,
                TimeUnit.MILLISECONDS);
        // Simulate a second invocation of Activity#onSaveInstanceState.
        doReturn(true).when(mWebContents).isFocusedElementEditable();
        doReturn(false).when(mKeyboardVisibilityDelegate).isKeyboardShowing(any(), any());
        savedInstanceState = new Bundle();
        mFoldTransitionController.saveUiState(savedInstanceState, /* didChangeTabletMode= */ true);

        Assert.assertTrue("Saved instance state should contain KEYBOARD_VISIBILITY_STATE.",
                savedInstanceState.containsKey(KEYBOARD_VISIBILITY_STATE));
        Assert.assertTrue("KEYBOARD_VISIBILITY_STATE in the saved instance state should be true.",
                savedInstanceState.getBoolean(KEYBOARD_VISIBILITY_STATE));
        Assert.assertFalse("|mKeyboardVisibleDuringFoldTransition| should be reset.",
                mFoldTransitionController.getKeyboardVisibleDuringFoldTransitionForTesting());
        Assert.assertNull("|mKeyboardVisibilityTimestamp| should be reset.",
                mFoldTransitionController.getKeyboardVisibilityTimestampForTesting());

        verify(mWebContents, times(2)).isFocusedElementEditable();
        verify(mKeyboardVisibilityDelegate, times(2))
                .isKeyboardShowing(mActivityTab.getContext(), mContentView);
    }

    @Test
    public void testSaveUiState_keyboardVisible_crbug1426678_stateValidityTimedOut()
            throws InterruptedException {
        doReturn(true).when(mWebContents).isFocusedElementEditable();
        doReturn(true).when(mKeyboardVisibilityDelegate).isKeyboardShowing(any(), any());
        Bundle savedInstanceState = new Bundle();
        mFoldTransitionController.saveUiState(savedInstanceState, /* didChangeTabletMode= */ false);

        Assert.assertTrue("Saved instance state should contain KEYBOARD_VISIBILITY_STATE.",
                savedInstanceState.containsKey(KEYBOARD_VISIBILITY_STATE));
        Assert.assertTrue("KEYBOARD_VISIBILITY_STATE in the saved instance state should be true.",
                savedInstanceState.getBoolean(KEYBOARD_VISIBILITY_STATE));
        Assert.assertTrue("|mKeyboardVisibleDuringFoldTransition| should be true.",
                mFoldTransitionController.getKeyboardVisibleDuringFoldTransitionForTesting());
        Assert.assertNotNull("|mKeyboardVisibilityTimestamp| should not be null.",
                mFoldTransitionController.getKeyboardVisibilityTimestampForTesting());

        ShadowSystemClock.advanceBy(FoldTransitionController.KEYBOARD_RESTORATION_TIMEOUT_MS + 1,
                TimeUnit.MILLISECONDS);
        // Simulate a second invocation of Activity#onSaveInstanceState.
        doReturn(true).when(mWebContents).isFocusedElementEditable();
        doReturn(false).when(mKeyboardVisibilityDelegate).isKeyboardShowing(any(), any());
        savedInstanceState = new Bundle();
        mFoldTransitionController.saveUiState(savedInstanceState, /* didChangeTabletMode= */ true);

        Assert.assertFalse("Saved instance state should not contain KEYBOARD_VISIBILITY_STATE.",
                savedInstanceState.containsKey(KEYBOARD_VISIBILITY_STATE));
        Assert.assertFalse("|mKeyboardVisibleDuringFoldTransition| should be reset.",
                mFoldTransitionController.getKeyboardVisibleDuringFoldTransitionForTesting());
        Assert.assertNull("|mKeyboardVisibilityTimestamp| should be reset.",
                mFoldTransitionController.getKeyboardVisibilityTimestampForTesting());

        verify(mWebContents, times(2)).isFocusedElementEditable();
        verify(mKeyboardVisibilityDelegate, times(2))
                .isKeyboardShowing(mActivityTab.getContext(), mContentView);
    }

    @Test
    public void testSaveUiState_keyboardNotVisible() {
        doReturn(false).when(mWebContents).isFocusedElementEditable();
        doReturn(false).when(mKeyboardVisibilityDelegate).isKeyboardShowing(any(), any());
        Bundle savedInstanceState = new Bundle();
        mFoldTransitionController.saveUiState(savedInstanceState, /* didChangeTabletMode= */ true);

        Assert.assertFalse("Saved instance state should not contain KEYBOARD_VISIBILITY_STATE.",
                savedInstanceState.containsKey(KEYBOARD_VISIBILITY_STATE));
    }

    @Test
    public void testRestoreUiState_urlBarFocused_layoutPendingShow() {
        String text = "hello";
        mFoldTransitionController.restoreUiState(createSavedInstanceState(
                /* didChangeTabletMode= */ true, /* urlBarFocused= */ true, text,
                /* keyboardVisible */ false));
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
                .setUrlBarFocusAndText(true, OmniboxFocusReason.FOLD_TRANSITION_RESTORATION, text);
    }

    @Test
    public void testRestoreUiState_urlBarFocused_layoutDoneShowing() {
        String text = "hello";
        // Assume that Layout#doneShowing is invoked before invocation of #restoreUiState.
        doReturn(true).when(mLayoutManager).isLayoutVisible(LayoutType.BROWSING);
        doReturn(false).when(mLayoutManager).isLayoutStartingToShow(LayoutType.BROWSING);
        mFoldTransitionController.restoreUiState(createSavedInstanceState(
                /* didChangeTabletMode= */ true, /* urlBarFocused= */ true, text,
                /* keyboardVisible */ true));
        verify(mToolbarManager)
                .setUrlBarFocusAndText(true, OmniboxFocusReason.FOLD_TRANSITION_RESTORATION, text);
        // Omnibox code should restore keyboard.
        verify(mKeyboardVisibilityDelegate, never()).showKeyboard(mContentView);
    }

    @Test
    public void testRestoreUiState_keyboardVisibleOnWebContentsFocus_layoutDoneShowing() {
        // Assume that Layout#doneShowing is invoked before invocation of #restoreUiState.
        doReturn(true).when(mLayoutManager).isLayoutVisible(LayoutType.BROWSING);
        doReturn(false).when(mLayoutManager).isLayoutStartingToShow(LayoutType.BROWSING);
        mFoldTransitionController.restoreUiState(createSavedInstanceState(
                /* didChangeTabletMode= */ true, /* urlBarFocused= */ false, null,
                /* keyboardVisible */ true));

        verify(mWebContents).scrollFocusedEditableNodeIntoView();
        verify(mKeyboardVisibilityDelegate).showKeyboard(mContentView);
    }

    @Test
    public void testRestoreUiState_urlBarNotFocused() {
        mFoldTransitionController.restoreUiState(createSavedInstanceState(
                /* didChangeTabletMode= */ true, /* urlBarFocused= */ false, null,
                /* keyboardVisible */ false));
        verify(mLayoutManager, never()).addObserver(any());
        verify(mToolbarManager, never()).setUrlBarFocusAndText(anyBoolean(), anyInt(), any());
    }

    @Test
    public void testRestoreUiState_didNotChangeTabletMode() {
        String text = "hello";
        mFoldTransitionController.restoreUiState(createSavedInstanceState(
                /* didChangeTabletMode= */ false, /* urlBarFocused= */ true, text,
                /* keyboardVisible */ false));
        verify(mLayoutManager, never()).addObserver(any());
        verify(mToolbarManager, never()).setUrlBarFocusAndText(anyBoolean(), anyInt(), any());
    }

    private Bundle createSavedInstanceState(boolean didChangeTabletMode, boolean urlBarFocused,
            String urlBarText, boolean keyboardVisible) {
        Bundle savedInstanceState = new Bundle();
        savedInstanceState.putBoolean(DID_CHANGE_TABLET_MODE, didChangeTabletMode);

        // Omnibox state keys.
        savedInstanceState.putBoolean(URL_BAR_FOCUS_STATE, urlBarFocused);
        if (urlBarText != null) {
            savedInstanceState.putString(URL_BAR_EDIT_TEXT, urlBarText);
        }

        // Keyboard state key(s).
        savedInstanceState.putBoolean(KEYBOARD_VISIBILITY_STATE, keyboardVisible);
        return savedInstanceState;
    }

    private void initializeController() {
        var toolbarManagerSupplier = new OneshotSupplierImpl<ToolbarManager>();
        toolbarManagerSupplier.set(mToolbarManager);
        var layoutManagerSupplier = new ObservableSupplierImpl<LayoutManager>();
        layoutManagerSupplier.set(mLayoutManager);
        mFoldTransitionController = new FoldTransitionController(
                toolbarManagerSupplier, layoutManagerSupplier, mActivityTabProvider, mHandler);
    }
}
