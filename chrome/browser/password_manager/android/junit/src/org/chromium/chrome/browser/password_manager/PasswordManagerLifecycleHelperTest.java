// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertSame;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;

/** Test class for {@link PasswordManagerLifecycleHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordManagerLifecycleHelperTest {
    private static final long sFakeNativePointer = 96024;

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private PasswordManagerLifecycleHelper.Natives mBridgeJniMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(PasswordManagerLifecycleHelperJni.TEST_HOOKS, mBridgeJniMock);
    }

    @Test
    public void testReuseInstance() {
        assertSame(
                PasswordManagerLifecycleHelper.getInstance(),
                PasswordManagerLifecycleHelper.getInstance());
    }

    @Test
    public void testSendNoNotificationWithoutObservers() {
        PasswordManagerLifecycleHelper.getInstance().onStartForegroundSession();
        verifyNoMoreInteractions(mBridgeJniMock);
    }

    @Test
    public void testNotifyForegroundSessionStart() {
        PasswordManagerLifecycleHelper.getInstance().registerObserver(sFakeNativePointer);
        PasswordManagerLifecycleHelper.getInstance().onStartForegroundSession();
        verify(mBridgeJniMock).onForegroundSessionStart(sFakeNativePointer);
    }

    @Test
    public void testDonNotifyAfterUnregister() {
        PasswordManagerLifecycleHelper.getInstance().registerObserver(sFakeNativePointer);
        PasswordManagerLifecycleHelper.getInstance().unregisterObserver(sFakeNativePointer);
        PasswordManagerLifecycleHelper.getInstance().onStartForegroundSession();
        verify(mBridgeJniMock, never()).onForegroundSessionStart(sFakeNativePointer);
    }
}
