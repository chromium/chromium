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
import static org.mockito.Mockito.verify;

import android.os.Bundle;
import android.os.Handler;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link FoldTransitionController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FoldTransitionControllerUnitTest {
    @Mock
    private ActivityTabProvider mActivityTabProvider;
    @Mock
    private Tab mActivityTab;
    @Mock
    private ToolbarManager mToolbarManager;
    @Mock
    private LayoutStateProvider mLayoutManager;
    @Mock
    private Handler mHandler;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn(mActivityTab).when(mActivityTabProvider).get();
        doReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.HTTP_URL)).when(mActivityTab).getUrl();
        doNothing().when(mToolbarManager).setUrlBarFocusAndText(anyBoolean(), anyInt(), any());
        doNothing().when(mLayoutManager).addObserver(any());
        doReturn(true).when(mLayoutManager).isLayoutStartingToShow(LayoutType.BROWSING);
    }

    @Test
    public void testSaveUiState_urlBarFocused() {
        String text = "hello";
        doReturn(true).when(mToolbarManager).isUrlBarFocused();
        doReturn(text).when(mToolbarManager).getUrlBarTextWithoutAutocomplete();
        Bundle savedInstanceState = new Bundle();
        FoldTransitionController.saveUiState(savedInstanceState, mToolbarManager,
                mActivityTabProvider, /* didChangeTabletMode= */ true);

        Assert.assertTrue("Saved instance state should contain URL_BAR_FOCUS_STATE.",
                savedInstanceState.containsKey(FoldTransitionController.URL_BAR_FOCUS_STATE));
        Assert.assertTrue("URL_BAR_FOCUS_STATE in the saved instance state should be true.",
                savedInstanceState.getBoolean(FoldTransitionController.URL_BAR_FOCUS_STATE));
        Assert.assertTrue("Saved instance state should contain URL_BAR_EDIT_TEXT.",
                savedInstanceState.containsKey(FoldTransitionController.URL_BAR_EDIT_TEXT));
        Assert.assertEquals("URL_BAR_EDIT_TEXT in the saved instance state should match.", text,
                savedInstanceState.getString(FoldTransitionController.URL_BAR_EDIT_TEXT));
    }

    @Test
    public void testSaveUiState_urlBarNotFocused() {
        doReturn(false).when(mToolbarManager).isUrlBarFocused();
        Bundle savedInstanceState = new Bundle();
        FoldTransitionController.saveUiState(savedInstanceState, mToolbarManager,
                mActivityTabProvider, /* didChangeTabletMode= */ true);

        Assert.assertFalse("Saved instance state should not contain URL_BAR_FOCUS_STATE.",
                savedInstanceState.containsKey(FoldTransitionController.URL_BAR_FOCUS_STATE));
        Assert.assertFalse("Saved instance state should not contain URL_BAR_EDIT_TEXT.",
                savedInstanceState.containsKey(FoldTransitionController.URL_BAR_EDIT_TEXT));
    }

    @Test
    public void testRestoreUiState_urlBarFocused_layoutPendingShow() {
        String text = "hello";
        FoldTransitionController.restoreUiState(
                createSavedInstanceState(
                        /* didChangeTabletMode= */ true, /* urlBarFocused= */ true, text),
                mToolbarManager, mLayoutManager, mHandler);
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
        FoldTransitionController.restoreUiState(
                createSavedInstanceState(
                        /* didChangeTabletMode= */ true, /* urlBarFocused= */ true, text),
                mToolbarManager, mLayoutManager, mHandler);
        verify(mToolbarManager)
                .setUrlBarFocusAndText(true, OmniboxFocusReason.FOLD_TRANSITION_RESTORATION, text);
    }

    @Test
    public void testRestoreUiState_urlBarNotFocused() {
        FoldTransitionController.restoreUiState(
                createSavedInstanceState(
                        /* didChangeTabletMode= */ true, /* urlBarFocused= */ false, null),
                mToolbarManager, mLayoutManager, mHandler);
        verify(mLayoutManager, never()).addObserver(any());
        verify(mToolbarManager, never()).setUrlBarFocusAndText(anyBoolean(), anyInt(), any());
    }

    @Test
    public void testRestoreUiState_didNotChangeTabletMode() {
        String text = "hello";
        FoldTransitionController.restoreUiState(
                createSavedInstanceState(
                        /* didChangeTabletMode= */ false, /* urlBarFocused= */ true, text),
                mToolbarManager, mLayoutManager, mHandler);
        verify(mLayoutManager, never()).addObserver(any());
        verify(mToolbarManager, never()).setUrlBarFocusAndText(anyBoolean(), anyInt(), any());
    }

    private Bundle createSavedInstanceState(
            boolean didChangeTabletMode, boolean urlBarFocused, String urlBarText) {
        Bundle savedInstanceState = new Bundle();
        savedInstanceState.putBoolean(
                FoldTransitionController.DID_CHANGE_TABLET_MODE, didChangeTabletMode);
        savedInstanceState.putBoolean(FoldTransitionController.URL_BAR_FOCUS_STATE, urlBarFocused);
        if (urlBarText != null) {
            savedInstanceState.putString(FoldTransitionController.URL_BAR_EDIT_TEXT, urlBarText);
        }
        return savedInstanceState;
    }
}
