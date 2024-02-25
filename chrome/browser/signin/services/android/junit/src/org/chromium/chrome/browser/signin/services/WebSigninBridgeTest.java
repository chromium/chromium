// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.base.GoogleServiceAuthError.State;

/** Unit tests for {@link WebSigninBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class WebSigninBridgeTest {
    private static final CoreAccountInfo CORE_ACCOUNT_INFO =
            CoreAccountInfo.createFromEmailAndGaiaId("user@domain.com", "gaia-id-user");
    private static final long NATIVE_WEB_SIGNIN_BRIDGE = 1000L;

    @Rule public final JniMocker mocker = new JniMocker();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebSigninBridge.Natives mNativeMock;

    @Mock private Profile mProfileMock;

    @Mock private WebSigninBridge.Listener mListenerMock;

    private final WebSigninBridge.Factory mFactory = new WebSigninBridge.Factory();

    @Before
    public void setUp() {
        mocker.mock(WebSigninBridgeJni.TEST_HOOKS, mNativeMock);
        when(mNativeMock.create(mProfileMock, CORE_ACCOUNT_INFO, mListenerMock))
                .thenReturn(NATIVE_WEB_SIGNIN_BRIDGE);
    }

    @Test
    public void testFactoryCreate() {
        WebSigninBridge webSigninBridge =
                mFactory.create(mProfileMock, CORE_ACCOUNT_INFO, mListenerMock);
        Assert.assertNotNull("Factory#create should not return null!", webSigninBridge);
        verify(mNativeMock).create(mProfileMock, CORE_ACCOUNT_INFO, mListenerMock);
    }

    @Test
    public void testDestroy() {
        mFactory.create(mProfileMock, CORE_ACCOUNT_INFO, mListenerMock).destroy();
        verify(mNativeMock).destroy(NATIVE_WEB_SIGNIN_BRIDGE);
    }

    @Test
    public void testOnSigninSucceed() {
        WebSigninBridge.onSigninSucceeded(mListenerMock);
        verify(mListenerMock).onSigninSucceeded();
        verify(mListenerMock, never()).onSigninFailed(any());
    }

    @Test
    public void testOnSigninFailed() {
        final GoogleServiceAuthError error = new GoogleServiceAuthError(State.CONNECTION_FAILED);
        WebSigninBridge.onSigninFailed(mListenerMock, error);
        verify(mListenerMock).onSigninFailed(error);
        verify(mListenerMock, never()).onSigninSucceeded();
    }
}
