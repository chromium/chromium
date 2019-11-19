// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.mockito.AdditionalMatchers.or;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tab.Tab.TabHidingType.CHANGED_TABS;
import static org.chromium.chrome.browser.tabmodel.TabSelectionType.FROM_NEW;
import static org.chromium.chrome.browser.tabmodel.TabSelectionType.FROM_USER;

import android.app.Activity;
import android.support.test.filters.SmallTest;

import com.google.android.libraries.feed.api.client.stream.Stream;

import org.junit.After;
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
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;

/**
 * Unit tests for {@link StreamLifecycleManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpStreamLifecycleManagerTest {
    @Mock
    private Activity mActivity;
    @Mock
    private Tab mTab;
    @Mock
    private Stream mStream;
    @Mock
    private PrefServiceBridge mPrefServiceBridge;

    private NtpStreamLifecycleManager mNtpStreamLifecycleManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Initialize a test instance for PrefServiceBridge.
        when(mPrefServiceBridge.getBoolean(anyInt())).thenReturn(true);
        doNothing().when(mPrefServiceBridge).setBoolean(anyInt(), anyBoolean());
        PrefServiceBridge.setInstanceForTesting(mPrefServiceBridge);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        mNtpStreamLifecycleManager = new NtpStreamLifecycleManager(mStream, mActivity, mTab);
        verify(mStream, times(1)).onCreate(or(any(String.class), isNull()));
    }

    @After
    public void tearDown() {
        PrefServiceBridge.setInstanceForTesting(null);
    }

    @Test
    @SmallTest
    public void testShow() {
        // Verify that onShow is not called before activity started.
        when(mTab.isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_NEW);
        verify(mStream, times(0)).onShow();

        // Verify that onShow is not called when Tab is hidden.
        when(mTab.isHidden()).thenReturn(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        verify(mStream, times(0)).onShow();

        // Verify that onShow is called when Tab is shown and activity is started.
        when(mTab.isHidden()).thenReturn(false);
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
        when(mPrefServiceBridge.getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE)).thenReturn(false);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        when(mTab.isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_NEW);
        verify(mStream, times(0)).onShow();

        // Verify that onShow is called when articles are set shown by the user.
        when(mPrefServiceBridge.getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE)).thenReturn(true);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_NEW);
        verify(mStream, times(1)).onShow();

        // Verify that onHide is called after tab is hidden.
        mNtpStreamLifecycleManager.getTabObserverForTesting().onHidden(mTab, CHANGED_TABS);
        verify(mStream, times(1)).onHide();
    }

    @Test
    @SmallTest
    public void testActivate() {
        // Verify that stream is not active before activity resumed.
        when(mTab.isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        verify(mStream, times(1)).onShow();
        verify(mStream, times(0)).onActive();

        // Verify that stream is not active before tab is user interactable.
        when(mTab.isUserInteractable()).thenReturn(false);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        verify(mStream, times(1)).onShow();
        verify(mStream, times(0)).onActive();

        // Verify that stream is active when tab is user interactable and activity is resumed.
        when(mTab.isUserInteractable()).thenReturn(true);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onInteractabilityChanged(true);
        verify(mStream, times(1)).onShow();
        verify(mStream, times(1)).onActive();

        // When the Stream is active, it won't call Stream#onShow() again.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        verify(mStream, times(1)).onShow();

        // When the Stream is active, it won't call Stream#onActive() again.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        verify(mStream, times(1)).onActive();
    }

    @Test
    @SmallTest
    public void testActivateAfterCreateAndHideAfterActivate() {
        // Activate the stream from created state.
        InOrder inOrder = Mockito.inOrder(mStream);
        when(mTab.isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        inOrder.verify(mStream).onShow();
        inOrder.verify(mStream).onActive();
        verify(mStream, times(1)).onShow();
        verify(mStream, times(1)).onActive();

        // Verify that the stream is deactivated before hidden.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        inOrder.verify(mStream).onInactive();
        inOrder.verify(mStream).onHide();
        verify(mStream, times(1)).onShow();
        verify(mStream, times(1)).onActive();
        verify(mStream, times(1)).onInactive();
        verify(mStream, times(1)).onHide();
    }

    @Test
    @SmallTest
    public void testDeactivate() {
        // Verify that the Stream cannot be set inactive from created.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        verify(mStream, times(0)).onInactive();

        // Show the stream.
        when(mTab.isHidden()).thenReturn(false);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        verify(mStream, times(1)).onShow();

        // Verify that the Stream cannot be set inactive from shown.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        verify(mStream, times(0)).onInactive();

        // Activate the stream.
        when(mTab.isUserInteractable()).thenReturn(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        verify(mStream, times(1)).onActive();

        // Verify that the Stream can be set inactive from active on activity paused.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        verify(mStream, times(1)).onInactive();

        // When the Stream is inactive, it won't call Stream#onInactive() again.
        mNtpStreamLifecycleManager.getTabObserverForTesting().onInteractabilityChanged(false);
        verify(mStream, times(1)).onInactive();

        // Verify that the Stream cannot be set shown from inactive.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        verify(mStream, times(1)).onShow();

        // Verify that the Stream can be set active from inactive.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        verify(mStream, times(2)).onActive();

        // Verify that the Stream can be set inactive from active on tab interactability changed.
        mNtpStreamLifecycleManager.getTabObserverForTesting().onInteractabilityChanged(false);
        verify(mStream, times(2)).onInactive();
    }

    @Test
    @SmallTest
    public void testHideFromActivityStopped() {
        // Activate the Stream.
        when(mTab.isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        verify(mStream, times(1)).onShow();
        verify(mStream, times(1)).onActive();

        // Deactivate the Stream.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        verify(mStream, times(1)).onInactive();

        // Verify that the Stream can be set hidden from inactive.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        verify(mStream, times(1)).onHide();

        // Verify that the Stream cannot be set inactive from hidden.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        verify(mStream, times(1)).onInactive();

        // When the Stream is hidden, it won't call Stream#onHide() again.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        verify(mStream, times(1)).onHide();
    }

    @Test
    @SmallTest
    public void testHideFromTabHiddenAfterShow() {
        // Show the stream.
        when(mTab.isHidden()).thenReturn(false);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        verify(mStream, times(1)).onShow();

        // Verify that onActive and onInactive are skipped when Stream is set hidden from shown.
        mNtpStreamLifecycleManager.getTabObserverForTesting().onHidden(
                mTab, TabHidingType.CHANGED_TABS);
        verify(mStream, times(1)).onShow();
        verify(mStream, times(0)).onActive();
        verify(mStream, times(0)).onInactive();
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
        verify(mStream, times(0)).onActive();
        verify(mStream, times(0)).onInactive();
        verify(mStream, times(0)).onHide();
        verify(mStream, times(1)).onDestroy();
    }

    @Test
    @SmallTest
    public void testDestroyAfterActivate() {
        InOrder inOrder = Mockito.inOrder(mStream);
        when(mTab.isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);

        // Activate the Stream.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        inOrder.verify(mStream).onShow();
        inOrder.verify(mStream).onActive();
        verify(mStream, times(1)).onShow();
        verify(mStream, times(1)).onActive();

        // Verify that onInactive and onHide is called before onDestroy.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
        inOrder.verify(mStream).onInactive();
        inOrder.verify(mStream).onHide();
        inOrder.verify(mStream).onDestroy();
        verify(mStream, times(1)).onInactive();
        verify(mStream, times(1)).onHide();
        verify(mStream, times(1)).onDestroy();
    }

    @Test
    @SmallTest
    public void testFullActivityLifecycle() {
        InOrder inOrder = Mockito.inOrder(mStream);
        when(mTab.isHidden()).thenReturn(false);
        when(mTab.isUserInteractable()).thenReturn(true);

        // On activity start and resume (simulates app become foreground).
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        inOrder.verify(mStream).onShow();
        inOrder.verify(mStream).onActive();
        verify(mStream, times(1)).onShow();
        verify(mStream, times(1)).onActive();

        // On activity pause and then resume (simulates multi-window mode).
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        inOrder.verify(mStream).onInactive();
        inOrder.verify(mStream).onActive();
        verify(mStream, times(1)).onInactive();
        verify(mStream, times(2)).onActive();

        // On activity stop (simulates app switched to background).
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        inOrder.verify(mStream).onInactive();
        inOrder.verify(mStream).onHide();
        verify(mStream, times(2)).onInactive();
        verify(mStream, times(1)).onHide();

        // On activity start (simulates app switched back to foreground).
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        inOrder.verify(mStream).onShow();
        inOrder.verify(mStream).onActive();
        verify(mStream, times(2)).onShow();
        verify(mStream, times(3)).onActive();

        // On activity pause, stop, and destroy (simulates app removed from Android recents).
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
        inOrder.verify(mStream).onInactive();
        inOrder.verify(mStream).onHide();
        inOrder.verify(mStream).onDestroy();
        verify(mStream, times(3)).onInactive();
        verify(mStream, times(2)).onHide();
        verify(mStream, times(1)).onDestroy();
    }

    @Test
    @SmallTest
    public void testFullTabLifecycle() {
        InOrder inOrder = Mockito.inOrder(mStream);

        // On new tab page created.
        when(mTab.isHidden()).thenReturn(true);
        when(mTab.isUserInteractable()).thenReturn(false);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        verify(mStream, times(0)).onShow();

        // On tab shown.
        when(mTab.isHidden()).thenReturn(false);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onShown(mTab, FROM_NEW);
        inOrder.verify(mStream).onShow();
        verify(mStream, times(1)).onShow();

        // On tab interactable.
        when(mTab.isUserInteractable()).thenReturn(true);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onInteractabilityChanged(true);
        inOrder.verify(mStream).onActive();
        verify(mStream, times(1)).onActive();

        // On tab un-interactable (simulates user enter the tab switcher).
        when(mTab.isUserInteractable()).thenReturn(false);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onInteractabilityChanged(false);
        inOrder.verify(mStream).onInactive();
        verify(mStream, times(1)).onInactive();

        // On tab interactable (simulates user exit the tab switcher).
        when(mTab.isUserInteractable()).thenReturn(true);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onInteractabilityChanged(true);
        inOrder.verify(mStream).onActive();
        verify(mStream, times(2)).onActive();

        // On tab un-interactable and hidden (simulates user switch to another tab).
        when(mTab.isHidden()).thenReturn(true);
        when(mTab.isUserInteractable()).thenReturn(false);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onInteractabilityChanged(false);
        mNtpStreamLifecycleManager.getTabObserverForTesting().onHidden(mTab, CHANGED_TABS);
        inOrder.verify(mStream).onInactive();
        inOrder.verify(mStream).onHide();
        verify(mStream, times(2)).onInactive();
        verify(mStream, times(1)).onHide();

        // On tab shown (simulates user switch back to this tab).
        when(mTab.isHidden()).thenReturn(false);
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
}
