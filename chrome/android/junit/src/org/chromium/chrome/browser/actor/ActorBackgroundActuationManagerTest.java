// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** Unit tests for {@link ActorBackgroundActuationManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActorBackgroundActuationManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String MESSAGE_ID_SUCCESS = "message_id_success";
    private static final String MESSAGE_ID_FAIL = "message_id_fail";
    private static final String MESSAGE_ID_CRASH = "message_id_crash";
    private static final String MESSAGE_ID_CANCELLED = "message_id_cancelled";
    private static final String TEST_URL = "about:blank";

    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorKeyedService;
    @Mock private OffscreenRenderingManager mOffscreenRenderingManager;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Tab mTab;

    private ActorBackgroundActuationManager mManager;

    @Before
    public void setUp() {
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
        OffscreenRenderingManager.setInstanceForTesting(mOffscreenRenderingManager);

        when(mOffscreenRenderingManager.getOffscreenWindow()).thenReturn(mWindowAndroid);
        TabBuilder.setTabForTesting(mTab);

        mManager = new ActorBackgroundActuationManager();
    }

    @After
    public void tearDown() {
        ProfileManager.resetForTesting();
        ActorKeyedServiceFactory.setForTesting(null);
        OffscreenRenderingManager.setInstanceForTesting(null);
        TabBuilder.setTabForTesting(null);
    }

    @Test
    public void testStartBackgroundActuation_Success() {
        mManager.startBackgroundActuation(mProfile, MESSAGE_ID_SUCCESS);

        // Verify offscreen rendering started
        verify(mOffscreenRenderingManager).startOffscreenRendering(eq(mTab), anyInt(), anyInt());

        // Capture and trigger page load success
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        observer.onPageLoadFinished(mTab, new GURL(TEST_URL));

        // Verify the tab was prepared and set on ActorKeyedService
        verify(mActorKeyedService).setPreparedBackgroundTab(mTab, MESSAGE_ID_SUCCESS);
        verify(mActorKeyedService, never()).notifyBackgroundSetupFailed(any());
    }

    @Test
    public void testStartBackgroundActuation_PageLoadFailed() {
        mManager.startBackgroundActuation(mProfile, MESSAGE_ID_FAIL);

        // Capture observer and trigger page load failure
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        observer.onPageLoadFailed(mTab, 404);

        // Verify setup failed notification was sent to native
        verify(mActorKeyedService).notifyBackgroundSetupFailed(MESSAGE_ID_FAIL);
        // Verify cleanup stopped offscreen rendering
        verify(mTab).removeObserver(observer);
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
    }

    @Test
    public void testStartBackgroundActuation_Crash() {
        mManager.startBackgroundActuation(mProfile, MESSAGE_ID_CRASH);

        // Capture observer and trigger renderer crash
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        observer.onCrash(mTab);

        // Verify setup failed and cleanup was executed
        verify(mActorKeyedService).notifyBackgroundSetupFailed(MESSAGE_ID_CRASH);
        verify(mTab).removeObserver(observer);
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
    }

    @Test
    public void testStartBackgroundActuation_CancelledBeforeLoadFinished() {
        mManager.startBackgroundActuation(mProfile, MESSAGE_ID_CANCELLED);

        // Capture observer
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        // Cancel/Cleanup before load finished
        mManager.cleanupContext(MESSAGE_ID_CANCELLED);

        // Verify cleanup stopped offscreen rendering
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);

        // Simulate native destruction triggering onDestroyed
        observer.onDestroyed(mTab);

        // Verify observer removed itself
        verify(mTab).removeObserver(observer);

        // Now trigger page load finish
        observer.onPageLoadFinished(mTab, new GURL(TEST_URL));

        // Verify setPreparedBackgroundTab was NOT called because of our fast-guard
        verify(mActorKeyedService, never()).setPreparedBackgroundTab(any(), any());
    }
}
