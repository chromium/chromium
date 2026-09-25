// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tab.TabHidingType.CHANGED_TABS;
import static org.chromium.chrome.browser.tab.TabSelectionType.FROM_NEW;
import static org.chromium.chrome.browser.tab.TabSelectionType.FROM_USER;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link NtpFeedSurfaceLifecycleManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpFeedSurfaceLifecycleManagerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Activity mActivity;
    @Mock private Tab mTab;
    @Mock private PrefService mPrefService;
    @Mock private FeedSurfaceCoordinator mCoordinator;
    @Mock private WebContents mWebContents;
    @Mock private NavigationController mNavigationController;
    @Mock private NavigationEntry mNavigationEntry;

    private NtpFeedSurfaceLifecycleManager mNtpStreamLifecycleManager;

    @Before
    public void setUp() {
        // Initialize a test instance for PrefService.
        when(mPrefService.getBoolean(anyString())).thenReturn(true);
        doNothing().when(mPrefService).setBoolean(anyString(), anyBoolean());
        NtpFeedSurfaceLifecycleManager.setPrefServiceForTesting(mPrefService);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        mNtpStreamLifecycleManager =
                new NtpFeedSurfaceLifecycleManager(mActivity, mTab, mCoordinator);
    }

    @Test
    public void testShow() {
        // Verify that onSurfaceOpened is not called before activity started.
        when(mTab.isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_NEW);
        verify(mCoordinator, times(0)).onSurfaceOpened();

        // Verify that onSurfaceOpened is not called when Tab is hidden.
        when(mTab.isHidden()).thenReturn(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        verify(mCoordinator, times(0)).onSurfaceOpened();

        // Verify that onSurfaceOpened is called when Tab is shown and activity is started.
        when(mTab.isHidden()).thenReturn(false);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_NEW);
        verify(mCoordinator, times(1)).onSurfaceOpened();

        // When the surface is shown, it won't call FeedSurfaceCoordinator#onSurfaceOpened() again.
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_NEW);
        verify(mCoordinator, times(1)).onSurfaceOpened();
    }

    @Test
    public void testShow_ArticlesNotVisible() {
        // Verify that onSurfaceOpened is not called when articles are set hidden by the user.
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(false);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        when(mTab.isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_NEW);
        verify(mCoordinator, times(0)).onSurfaceOpened();

        // Verify that onSurfaceOpened is called when articles are set shown by the user.
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_NEW);
        verify(mCoordinator, times(1)).onSurfaceOpened();

        // Verify that onSurfaceClosed is called after tab is hidden.
        mNtpStreamLifecycleManager.getTabObserverForTesting().onHidden(mTab, CHANGED_TABS);
        verify(mCoordinator, times(1)).onSurfaceClosed();
    }

    @Test
    public void testHideFromActivityStopped() {
        // Activate the surface.
        when(mTab.isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        verify(mCoordinator, times(1)).onSurfaceOpened();

        // Deactivate the surface.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);

        // Verify that the surface can be set hidden from inactive.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        verify(mCoordinator, times(1)).onSurfaceClosed();
    }

    @Test
    public void testHideFromTabHiddenAfterShow() {
        // Show the surface.
        when(mTab.isHidden()).thenReturn(false);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        verify(mCoordinator, times(1)).onSurfaceOpened();

        // Hide the surface.
        mNtpStreamLifecycleManager
                .getTabObserverForTesting()
                .onHidden(mTab, TabHidingType.CHANGED_TABS);
        verify(mCoordinator, times(1)).onSurfaceOpened();
        verify(mCoordinator, times(1)).onSurfaceClosed();
    }

    @Test
    public void testDestroy() {
        // Verify that observer is removed on activity destroyed.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
        verify(mTab, times(1)).removeObserver(any());
    }

    @Test
    public void testDestroyAfterCreate() {
        // After the surface is destroyed, lifecycle methods should never be called. Directly
        // calling destroy here to simulate destroy() being called on FeedNewTabPage destroyed.
        mNtpStreamLifecycleManager.destroy();
        verify(mTab, times(1)).removeObserver(any());

        // Verify that lifecycle methods are not called after destroy.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
        verify(mCoordinator, times(0)).onSurfaceOpened();
        verify(mCoordinator, times(0)).onSurfaceClosed();
        verify(mTab, times(1)).removeObserver(any());
    }

    @Test
    public void testDestroyAfterActivate() {
        InOrder inOrder = Mockito.inOrder(mCoordinator, mTab);
        when(mTab.isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);

        // Activate the surface.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        inOrder.verify(mCoordinator).onSurfaceOpened();
        verify(mCoordinator, times(1)).onSurfaceOpened();

        // Verify that onSurfaceClosed is called before destroy finishes.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
        inOrder.verify(mCoordinator).onSurfaceClosed();
        inOrder.verify(mTab).removeObserver(any());
        verify(mCoordinator, times(1)).onSurfaceClosed();
        verify(mTab, times(1)).removeObserver(any());
    }

    @Test
    public void testFullActivityLifecycle() {
        InOrder inOrder = Mockito.inOrder(mCoordinator, mTab);
        when(mTab.isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);

        // On activity start and resume (simulates app become foreground).
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        inOrder.verify(mCoordinator).onSurfaceOpened();
        verify(mCoordinator, times(1)).onSurfaceOpened();

        // On activity pause and then resume (simulates multi-window mode).
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);

        // On activity stop (simulates app switched to background).
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        inOrder.verify(mCoordinator).onSurfaceClosed();
        verify(mCoordinator, times(1)).onSurfaceClosed();

        // On activity start (simulates app switched back to foreground).
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        inOrder.verify(mCoordinator).onSurfaceOpened();
        verify(mCoordinator, times(2)).onSurfaceOpened();

        // On activity pause, stop, and destroy (simulates app removed from Android recents).
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
        inOrder.verify(mCoordinator).onSurfaceClosed();
        inOrder.verify(mTab).removeObserver(any());
        verify(mCoordinator, times(2)).onSurfaceClosed();
        verify(mTab, times(1)).removeObserver(any());
    }

    @Test
    public void testFullTabLifecycle() {
        InOrder inOrder = Mockito.inOrder(mCoordinator, mTab);

        // On new tab page created.
        when(mTab.isHidden()).thenReturn(true);
        when(mTab.isUserInteractable()).thenReturn(false);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        verify(mCoordinator, times(0)).onSurfaceOpened();

        // On tab shown.
        when(mTab.isHidden()).thenReturn(false);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_NEW);
        inOrder.verify(mCoordinator).onSurfaceOpened();
        verify(mCoordinator, times(1)).onSurfaceOpened();

        // On tab interactable.
        when(mTab.isUserInteractable()).thenReturn(true);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onInteractabilityChanged(mTab, true);

        // On tab un-interactable (simulates user enter the tab switcher).
        when(mTab.isUserInteractable()).thenReturn(false);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onInteractabilityChanged(mTab, false);

        // On tab interactable (simulates user exit the tab switcher).
        when(mTab.isUserInteractable()).thenReturn(true);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onInteractabilityChanged(mTab, true);

        // On tab un-interactable and hidden (simulates user switch to another tab).
        when(mTab.isHidden()).thenReturn(true);
        when(mTab.isUserInteractable()).thenReturn(false);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onInteractabilityChanged(mTab, false);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onHidden(mTab, CHANGED_TABS);
        inOrder.verify(mCoordinator).onSurfaceClosed();
        verify(mCoordinator, times(1)).onSurfaceClosed();

        // On tab shown (simulates user switch back to this tab).
        when(mTab.isHidden()).thenReturn(false);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_USER);
        inOrder.verify(mCoordinator).onSurfaceOpened();
        verify(mCoordinator, times(2)).onSurfaceOpened();

        // On tab destroy (simulates user close the tab or navigate to another URL).
        mNtpStreamLifecycleManager.destroy();
        inOrder.verify(mCoordinator).onSurfaceClosed();
        inOrder.verify(mTab).removeObserver(any());
        verify(mCoordinator, times(2)).onSurfaceClosed();
        verify(mTab, times(1)).removeObserver(any());
    }

    @Test
    public void testHideOnInteractabilityChangedWhenDetached() {
        when(mTab.isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mNavigationController.getLastCommittedEntryIndex()).thenReturn(0);
        when(mNavigationController.getEntryAtIndex(0)).thenReturn(mNavigationEntry);
        when(mNavigationEntry.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mCoordinator.getSavedInstanceStateString()).thenReturn("saved_scroll_state");

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        verify(mCoordinator, times(1)).onSurfaceOpened();

        // When the Tab loses interactability because it is detached from its Activity (before its
        // NativePage view is detached and destroyed), NtpFeedSurfaceLifecycleManager should hide
        // the surface and save instance state to the navigation entry.
        when(mTab.isUserInteractable()).thenReturn(false);
        when(mTab.isDetachedFromActivity()).thenReturn(true);
        mNtpStreamLifecycleManager
                .getTabObserverForTesting()
                .onInteractabilityChanged(mTab, /* isInteractable= */ false);
        verify(mCoordinator, times(1)).onSurfaceClosed();
        verify(mNavigationController, times(1))
                .setEntryExtraData(0, "FeedSavedInstanceState", "saved_scroll_state");

        // A subsequent destroy() (after the NativePage view is detached from its parent) should not
        // call hide() or overwrite the saved instance state a second time.
        when(mCoordinator.getSavedInstanceStateString()).thenReturn("");
        mNtpStreamLifecycleManager.destroy();
        verify(mCoordinator, times(1)).onSurfaceClosed();
        verify(mNavigationController, times(1))
                .setEntryExtraData(anyInt(), anyString(), anyString());
    }

    @Test
    public void testPaused() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        verify(mCoordinator).onActivityPaused();
    }
}
