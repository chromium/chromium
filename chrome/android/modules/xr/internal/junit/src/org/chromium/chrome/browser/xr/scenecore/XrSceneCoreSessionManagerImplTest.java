// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import androidx.activity.ComponentActivity;
import androidx.xr.runtime.Session;
import androidx.xr.runtime.SessionCreateResult;
import androidx.xr.runtime.SessionCreateSuccess;
import androidx.xr.runtime.internal.JxrRuntime;
import androidx.xr.scenecore.ActivitySpace;
import androidx.xr.scenecore.Scene;
import androidx.xr.scenecore.SessionExt;
import androidx.xr.scenecore.testing.FakeSceneRuntime;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link XrSceneCoreSessionManagerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class XrSceneCoreSessionManagerImplTest {

    private ComponentActivity mActivity;
    private ActivityController<ComponentActivity> mActivityController;
    @Mock private Runnable mCallback;

    private Session mSessionDelegate;
    private Scene mScene;
    private ActivitySpace mActivitySpace;
    private XrSceneCoreSessionManagerImpl mManager;
    private FakeSceneRuntime mFakeRuntime;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);

        mActivityController = Robolectric.buildActivity(ComponentActivity.class);
        mActivity = spy(mActivityController.create().start().get());

        SessionCreateResult result = Session.create(mActivity);
        assertTrue(result instanceof SessionCreateSuccess);
        mSessionDelegate = ((SessionCreateSuccess) result).getSession();

        for (JxrRuntime runtime : mSessionDelegate.getRuntimes()) {
            if (runtime instanceof FakeSceneRuntime) {
                mFakeRuntime = (FakeSceneRuntime) runtime;
                break;
            }
        }
        assertNotNull(mFakeRuntime);

        mScene = SessionExt.getScene(mSessionDelegate);
        mActivitySpace = mScene.getActivitySpace();

        mManager = new XrSceneCoreSessionManagerImpl(mActivity, mSessionDelegate);

        if (mManager.isXrFullSpaceMode()) {
            mScene.requestHomeSpaceMode();
            ShadowLooper.idleMainLooper();
        }
    }

    @Test
    public void testCreate() {
        assertNotNull(mManager);
        assertFalse(mManager.isXrFullSpaceMode());
    }

    @Test
    public void testRequestSpaceModeChange_Success() {
        when(mActivity.hasWindowFocus()).thenReturn(true);

        boolean result = mManager.requestSpaceModeChange(true, mCallback);
        ShadowLooper.idleMainLooper();

        assertTrue(result);
        assertTrue(mManager.isXrFullSpaceMode());
    }

    @Test
    public void testRequestSpaceModeChange_NoFocus() {
        when(mActivity.hasWindowFocus()).thenReturn(false);

        boolean result = mManager.requestSpaceModeChange(true, mCallback);
        ShadowLooper.idleMainLooper();

        assertFalse(result);
        assertFalse(mManager.isXrFullSpaceMode());
    }

    @Test
    public void testRequestSpaceModeChange_AlreadyInMode() {
        when(mActivity.hasWindowFocus()).thenReturn(true);

        // Make it full space first
        mManager.requestSpaceModeChange(true, mCallback);
        ShadowLooper.idleMainLooper();
        assertTrue(mManager.isXrFullSpaceMode());

        boolean result =
                mManager.requestSpaceModeChange(/* requestFullSpaceMode= */ true, mCallback);

        assertFalse(result);
    }

    @Test
    public void testRequestSpaceModeChange_HomeMode() {
        when(mActivity.hasWindowFocus()).thenReturn(true);

        // Make it full space first
        mManager.requestSpaceModeChange(true, mCallback);
        ShadowLooper.idleMainLooper();
        assertTrue(mManager.isXrFullSpaceMode());

        boolean result =
                mManager.requestSpaceModeChange(/* requestFullSpaceMode= */ false, mCallback);
        ShadowLooper.idleMainLooper();

        assertTrue(result);
        assertFalse(mManager.isXrFullSpaceMode());
    }
}
