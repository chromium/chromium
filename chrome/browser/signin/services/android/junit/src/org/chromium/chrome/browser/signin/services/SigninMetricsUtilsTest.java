// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.os.Looper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.task.test.BackgroundShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;

/**
 * Unit tests for {@link SigninMetricsUtils}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {BackgroundShadowAsyncTask.class})
public class SigninMetricsUtilsTest {
    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final JniMocker mocker = new JniMocker();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private SigninMetricsUtils.Natives mSigninMetricsUtilsNativeMock;

    @Before
    public void setUp() {
        initMocks(this);
        mocker.mock(SigninMetricsUtilsJni.TEST_HOOKS, mSigninMetricsUtilsNativeMock);
        mAccountManagerTestRule.addAccount("test@gmail.com");
    }

    @Test
    public void testLogSigninAfterDismissalTaskPostedToNative() throws Exception {
        SigninMetricsUtils.logWebSignin();
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper shadowLooper = Shadows.shadowOf(Looper.myLooper());
        // Each call to runToEndOfTasks() processes one posted task Scheduler.
        final int expectedCallCount = 8;
        for (int count = 0; count < expectedCallCount + 1; count++) {
            shadowLooper.runToEndOfTasks();
        }
        verify(mSigninMetricsUtilsNativeMock, times(expectedCallCount)).logWebSignin(any());
    }

    @Test
    public void testLogSigninAfterDismissalTaskNotPostedToNativeAfterWebSignIn() throws Exception {
        when(mSigninMetricsUtilsNativeMock.logWebSignin(any())).thenReturn(true);
        SigninMetricsUtils.logWebSignin();
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper shadowLooper = Shadows.shadowOf(Looper.myLooper());
        // Each call to runToEndOfTasks() processes one posted task Scheduler.
        final int expectedCallCount = 1;
        for (int count = 0; count < expectedCallCount + 1; count++) {
            shadowLooper.runToEndOfTasks();
        }
        verify(mSigninMetricsUtilsNativeMock, times(expectedCallCount)).logWebSignin(any());
    }
}