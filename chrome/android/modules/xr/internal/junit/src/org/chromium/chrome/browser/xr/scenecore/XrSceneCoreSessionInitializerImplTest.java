// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;

/** Tests for {@link XrSceneCoreSessionInitializerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class XrSceneCoreSessionInitializerImplTest {

    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private XrSceneCoreSessionManager mXrSceneCoreSessionManager;

    private XrSceneCoreSessionInitializerImpl mInitializer;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mInitializer =
                new XrSceneCoreSessionInitializerImpl(
                        mLifecycleDispatcher, mXrSceneCoreSessionManager);
    }

    @Test
    public void testCreate() {
        assertNotNull(mInitializer);
    }

    @Test
    public void testDestroy() {
        mInitializer.destroy();
        verify(mLifecycleDispatcher).unregister(mInitializer);
    }

    @Test
    public void testInitialize_InDesiredMode() {
        final boolean currentFullSpaceMode = true;
        final boolean desiredFullSpaceMode = true;

        when(mXrSceneCoreSessionManager.isXrFullSpaceMode()).thenReturn(currentFullSpaceMode);

        mInitializer.initialize(desiredFullSpaceMode);

        verify(mXrSceneCoreSessionManager, never()).requestSpaceModeChange(anyBoolean());
        verify(mLifecycleDispatcher, never()).register(any());
    }

    @Test
    public void testInitialize_NotInDesiredMode_Success() {
        final boolean currentFullSpaceMode = false;
        final boolean desiredFullSpaceMode = true;

        when(mXrSceneCoreSessionManager.isXrFullSpaceMode()).thenReturn(currentFullSpaceMode);
        when(mXrSceneCoreSessionManager.requestSpaceModeChange(/* requestFullSpaceMode= */ true))
                .thenReturn(/* requestSuccess= */ true);

        mInitializer.initialize(desiredFullSpaceMode);

        verify(mXrSceneCoreSessionManager).requestSpaceModeChange(true);
        verify(mLifecycleDispatcher, never()).register(any());
    }

    @Test
    public void testInitialize_NotInDesiredMode_Failure() {
        final boolean currentFullSpaceMode = false;
        final boolean desiredFullSpaceMode = true;

        when(mXrSceneCoreSessionManager.isXrFullSpaceMode()).thenReturn(currentFullSpaceMode);
        when(mXrSceneCoreSessionManager.requestSpaceModeChange(/* requestFullSpaceMode= */ true))
                .thenReturn(/* requestSuccess= */ false);

        mInitializer.initialize(desiredFullSpaceMode);

        verify(mXrSceneCoreSessionManager).requestSpaceModeChange(true);
        verify(mLifecycleDispatcher).register(mInitializer);
    }

    @Test
    public void testOnWindowFocusChanged_HasFocus() {
        when(mXrSceneCoreSessionManager.isXrFullSpaceMode()).thenReturn(false);
        when(mXrSceneCoreSessionManager.requestSpaceModeChange(/* requestFullSpaceMode= */ true))
                .thenReturn(/* requestSuccess= */ false); // Fail in init

        mInitializer.initialize(true); // Desired is true, current is false.

        // Assume initialization failed and registered listener
        mInitializer.onWindowFocusChanged(true);

        verify(mXrSceneCoreSessionManager, org.mockito.Mockito.times(2))
                .requestSpaceModeChange(true);
        verify(mLifecycleDispatcher).unregister(mInitializer);
    }
}
