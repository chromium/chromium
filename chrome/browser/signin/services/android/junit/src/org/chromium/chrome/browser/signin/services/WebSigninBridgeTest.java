// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

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

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GaiaId;
import org.chromium.components.signin.browser.WebSigninTrackerResult;

/** Unit tests for {@link WebSigninBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class WebSigninBridgeTest {
    private static final CoreAccountInfo CORE_ACCOUNT_INFO =
            CoreAccountInfo.createFromEmailAndGaiaId("user@domain.com", new GaiaId("gaia-id-user"));
    private static final long NATIVE_WEB_SIGNIN_BRIDGE = 1000L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebSigninBridge.Natives mNativeMock;

    @Mock private Profile mProfileMock;

    @Mock private Callback<@WebSigninTrackerResult Integer> mCallbackMock;

    private final WebSigninBridge.Factory mFactory = new WebSigninBridge.Factory();

    @Before
    public void setUp() {
        WebSigninBridgeJni.setInstanceForTesting(mNativeMock);
        when(mNativeMock.create(mProfileMock, CORE_ACCOUNT_INFO, mCallbackMock))
                .thenReturn(NATIVE_WEB_SIGNIN_BRIDGE);
    }

    @Test
    public void testFactoryCreate() {
        WebSigninBridge webSigninBridge =
                mFactory.create(mProfileMock, CORE_ACCOUNT_INFO, mCallbackMock);
        Assert.assertNotNull("Factory#create should not return null!", webSigninBridge);
        verify(mNativeMock).create(mProfileMock, CORE_ACCOUNT_INFO, mCallbackMock);
    }

    @Test
    public void testDestroy() {
        mFactory.create(mProfileMock, CORE_ACCOUNT_INFO, mCallbackMock).destroy();
        verify(mNativeMock).destroy(NATIVE_WEB_SIGNIN_BRIDGE);
    }

    @Test
    public void testOnSigninSucceed() {
        WebSigninBridge.onSigninResult(mCallbackMock, WebSigninTrackerResult.SUCCESS);
        verify(mCallbackMock).onResult(WebSigninTrackerResult.SUCCESS);
    }

    @Test
    public void testOnSigninFailed() {
        WebSigninBridge.onSigninResult(mCallbackMock, WebSigninTrackerResult.OTHER_ERROR);
        verify(mCallbackMock).onResult(WebSigninTrackerResult.OTHER_ERROR);
    }
}
