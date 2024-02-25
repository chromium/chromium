// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.mockito.AdditionalMatchers.or;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tab.TabHidingType.CHANGED_TABS;
import static org.chromium.chrome.browser.tab.TabSelectionType.FROM_NEW;
import static org.chromium.chrome.browser.tab.TabSelectionType.FROM_USER;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.components.prefs.PrefService;

/** Unit tests for {@link FeedSurfaceLifecycleManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpFeedSurfaceLifecycleManagerTest {
    @Mock private Activity mActivity;
    @Mock private Tab mTab;
    @Mock private Stream mStream;
    @Mock private PrefService mPrefService;
    @Mock private FeedSurfaceCoordinator mCoordinator;
    @Mock private FeedReliabilityLogger mFeedReliabilityLogger;

    private NtpFeedSurfaceLifecycleManager mNtpStreamLifecycleManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Initialize a test instance for PrefService.
        when(mPrefService.getBoolean(anyString())).thenReturn(true);
        doNothing().when(mPrefService).setBoolean(anyString(), anyBoolean());
        NtpFeedSurfaceLifecycleManager.setPrefServiceForTesting(mPrefService);
        when(mCoordinator.getFeedReliabilityLogger()).thenReturn(mFeedReliabilityLogger);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        mNtpStreamLifecycleManager =
                new NtpFeedSurfaceLifecycleManager(mActivity, mTab, mCoordinator);
        verify(mStream, times(1)).onCreate(or(any(String.class), isNull()));
    }

    @Test
    @SmallTest
    public void testShow() {
        // Verify that onShow is not called before activity started.
        when((mTab).isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_NEW);
        verify(mStream, times(0)).onShow();

        // Verify that onShow is not called when Tab is hidden.
        when((mTab).isHidden()).thenReturn(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        verify(mStream, times(0)).onShow();

        // Verify that onShow is called when Tab is shown and activity is started.
        when((mTab).isHidden()).thenReturn(false);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_NEW);
        verify(mStream, times(1)).onShow();

        // When the Stream is shown, it won't call Stream#onShow() again.
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_NEW);
        verify(mStream, times(1)).onShow();
    }

    @Test
    @SmallTest
    public void testShow_ArticlesNotVisible() {
        // Verify that onShow is not called when articles are set hidden by the user.
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(false);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        when((mTab).isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_NEW);
        verify(mStream, times(0)).onShow();

        // Verify that onShow is called when articles are set shown by the user.
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_NEW);
        verify(mStream, times(1)).onShow();

        // Verify that onHide is called after tab is hidden.
        mNtpStreamLifecycleManager.getTabObserverForTesting().onHidden(mTab, CHANGED_TABS);
        verify(mStream, times(1)).onHide();
    }

    @Test
    @SmallTest
    public void testHideFromActivityStopped() {
        // Activate the Stream.
        when((mTab).isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        verify(mStream, times(1)).onShow();

        // Deactivate the Stream.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);

        // Verify that the Stream can be set hidden from inactive.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        verify(mStream, times(1)).onHide();
    }

    @Test
    @SmallTest
    public void testHideFromTabHiddenAfterShow() {
        // Show the stream.
        when((mTab).isHidden()).thenReturn(false);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        verify(mStream, times(1)).onShow();

        // Hide the stream.
        mNtpStreamLifecycleManager
                .getTabObserverForTesting()
                .onHidden(mTab, TabHidingType.CHANGED_TABS);
        verify(mStream, times(1)).onShow();
        verify(mStream, times(1)).onHide();
    }

    @Test
    @SmallTest
    public void testDestroy() {
        // Verify that Stream#onDestroy is called on activity destroyed.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
        verify(mStream, times(1)).onDestroy();
    }

    @Test
    @SmallTest
    public void testDestroyAfterCreate() {
        // After the Stream is destroyed, lifecycle methods should never be called. Directly calling
        // destroy here to simulate destroy() being called on FeedNewTabPage destroyed.
        mNtpStreamLifecycleManager.destroy();
        verify(mStream, times(1)).onDestroy();

        // Verify that lifecycle methods are not called after destroy.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
        verify(mStream, times(0)).onShow();
        verify(mStream, times(0)).onHide();
        verify(mStream, times(1)).onDestroy();
    }

    @Test
    @SmallTest
    public void testDestroyAfterActivate() {
        InOrder inOrder = Mockito.inOrder(mStream);
        when((mTab).isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);

        // Activate the Stream.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        inOrder.verify(mStream).onShow();
        verify(mStream, times(1)).onShow();

        // Verify that onInactive and onHide is called before onDestroy.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
        inOrder.verify(mStream).onHide();
        inOrder.verify(mStream).onDestroy();
        verify(mStream, times(1)).onHide();
        verify(mStream, times(1)).onDestroy();
    }

    @Test
    @SmallTest
    public void testFullActivityLifecycle() {
        InOrder inOrder = Mockito.inOrder(mStream);
        when((mTab).isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);

        // On activity start and resume (simulates app become foreground).
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        inOrder.verify(mStream).onShow();
        verify(mStream, times(1)).onShow();

        // On activity pause and then resume (simulates multi-window mode).
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);

        // On activity stop (simulates app switched to background).
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        inOrder.verify(mStream).onHide();
        verify(mStream, times(1)).onHide();

        // On activity start (simulates app switched back to foreground).
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        inOrder.verify(mStream).onShow();
        verify(mStream, times(2)).onShow();

        // On activity pause, stop, and destroy (simulates app removed from Android recents).
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
        inOrder.verify(mStream).onHide();
        inOrder.verify(mStream).onDestroy();
        verify(mStream, times(2)).onHide();
        verify(mStream, times(1)).onDestroy();
    }

    @Test
    @SmallTest
    public void testFullTabLifecycle() {
        InOrder inOrder = Mockito.inOrder(mStream);

        // On new tab page created.
        when((mTab).isHidden()).thenReturn(true);
        when(mTab.isUserInteractable()).thenReturn(false);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        verify(mStream, times(0)).onShow();

        // On tab shown.
        when((mTab).isHidden()).thenReturn(false);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_NEW);
        inOrder.verify(mStream).onShow();
        verify(mStream, times(1)).onShow();

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
        when((mTab).isHidden()).thenReturn(true);
        when(mTab.isUserInteractable()).thenReturn(false);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onInteractabilityChanged(mTab, false);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onHidden(mTab, CHANGED_TABS);
        inOrder.verify(mStream).onHide();
        verify(mStream, times(1)).onHide();

        // On tab shown (simulates user switch back to this tab).
        when((mTab).isHidden()).thenReturn(false);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_USER);
        inOrder.verify(mStream).onShow();
        verify(mStream, times(2)).onShow();

        // On tab destroy (simulates user close the tab or navigate to another URL).
        mNtpStreamLifecycleManager.destroy();
        inOrder.verify(mStream).onHide();
        inOrder.verify(mStream).onDestroy();
        verify(mStream, times(2)).onHide();
        verify(mStream, times(1)).onDestroy();
    }

    @Test
    @SmallTest
    public void testPaused() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        verify(mCoordinator).onActivityPaused();
    }
}
