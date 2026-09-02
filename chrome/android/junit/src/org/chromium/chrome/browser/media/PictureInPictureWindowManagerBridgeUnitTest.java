// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link PictureInPictureWindowManagerBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PictureInPictureWindowManagerBridgeUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PictureInPictureWindowManagerBridge.Natives mBridgeNatives;
    @Mock private Callback<Boolean> mObserver;

    @Before
    public void setUp() {
        PictureInPictureWindowManagerBridgeJni.setInstanceForTesting(mBridgeNatives);
        PictureInPictureWindowManagerBridge.resetForTesting();
    }

    @After
    public void tearDown() {
        PictureInPictureWindowManagerBridge.getIsPictureInPictureShowingSupplier()
                .removeObserver(mObserver);
        PictureInPictureWindowManagerBridge.resetForTesting();
        PictureInPictureWindowManagerBridgeJni.setInstanceForTesting(null);
    }

    @Test
    public void testInitialState_notShowing() {
        assertFalse(
                PictureInPictureWindowManagerBridge.getIsPictureInPictureShowingSupplier().get());
    }

    @Test
    public void testInitializeWithNative_isInPipTrue() {
        when(mBridgeNatives.isInPictureInPicture()).thenReturn(true);

        PictureInPictureWindowManagerBridge.initializeWithNative();

        verify(mBridgeNatives).isInPictureInPicture();
        assertTrue(
                PictureInPictureWindowManagerBridge.getIsPictureInPictureShowingSupplier().get());
    }

    @Test
    public void testInitializeWithNative_isInPipFalse() {
        when(mBridgeNatives.isInPictureInPicture()).thenReturn(false);

        PictureInPictureWindowManagerBridge.initializeWithNative();

        verify(mBridgeNatives).isInPictureInPicture();
        assertFalse(
                PictureInPictureWindowManagerBridge.getIsPictureInPictureShowingSupplier().get());
    }

    @Test
    public void testStateChanged_updatesSupplierAndNotifiesObserver() {
        PictureInPictureWindowManagerBridge.getIsPictureInPictureShowingSupplier()
                .addSyncObserver(mObserver);

        // Native calls onPictureInPictureStateChanged(true)
        PictureInPictureWindowManagerBridge.onPictureInPictureStateChanged(true);
        assertTrue(
                PictureInPictureWindowManagerBridge.getIsPictureInPictureShowingSupplier().get());
        verify(mObserver).onResult(true);

        // Native calls onPictureInPictureStateChanged(false)
        PictureInPictureWindowManagerBridge.onPictureInPictureStateChanged(false);
        assertFalse(
                PictureInPictureWindowManagerBridge.getIsPictureInPictureShowingSupplier().get());
        verify(mObserver).onResult(false);
    }
}
