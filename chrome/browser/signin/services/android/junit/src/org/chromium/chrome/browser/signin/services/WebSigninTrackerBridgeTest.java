// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.jni_zero.JniUniquePtr;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.WebSigninTrackerBridge.NativeWebSigninTrackerBridge;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.browser.WebSigninTrackerResult;
import org.chromium.google_apis.gaia.GaiaId;

/** Unit tests for {@link WebSigninTrackerBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class WebSigninTrackerBridgeTest {
    private static final CoreAccountInfo CORE_ACCOUNT_INFO =
            CoreAccountInfo.createFromEmailAndGaiaId("user@domain.com", new GaiaId("gaia-id-user"));
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebSigninTrackerBridge.Natives mNativeMock;
    private JniUniquePtr<NativeWebSigninTrackerBridge> mNativeWebSigninTrackerBridge;

    @Mock private Profile mProfileMock;

    @Mock private Callback<@WebSigninTrackerResult Integer> mCallbackMock;

    @Captor
    private ArgumentCaptor<Callback<@WebSigninTrackerResult Integer>> mWebSigninCallbackCaptor;

    private final WebSigninTrackerBridge.Factory mFactory = new WebSigninTrackerBridge.Factory();

    @Before
    public void setUp() {
        WebSigninTrackerBridgeJni.setInstanceForTesting(mNativeMock);
        mNativeWebSigninTrackerBridge = JniUniquePtr.createForTesting(1L);
        when(mNativeMock.createWithCoreAccountId(
                        eq(mProfileMock), eq(CORE_ACCOUNT_INFO.getId()), any()))
                .thenReturn(mNativeWebSigninTrackerBridge);
    }

    @After
    public void tearDown() {
        if (mNativeWebSigninTrackerBridge != null) {
            mNativeWebSigninTrackerBridge.destroy();
        }
    }

    @Test
    public void testFactoryCreate() {
        WebSigninTrackerBridge webSigninTrackerBridge =
                mFactory.createWithCoreAccountId(
                        mProfileMock, CORE_ACCOUNT_INFO.getId(), mCallbackMock);
        Assert.assertNotNull("Factory#create should not return null!", webSigninTrackerBridge);
        verify(mNativeMock)
                .createWithCoreAccountId(eq(mProfileMock), eq(CORE_ACCOUNT_INFO.getId()), any());
    }

    @Test
    public void testDestroy() {
        mFactory.createWithCoreAccountId(mProfileMock, CORE_ACCOUNT_INFO.getId(), mCallbackMock);

        verify(mNativeMock)
                .createWithCoreAccountId(
                        eq(mProfileMock),
                        eq(CORE_ACCOUNT_INFO.getId()),
                        mWebSigninCallbackCaptor.capture());

        mWebSigninCallbackCaptor.getValue().onResult(WebSigninTrackerResult.SUCCESS);

        mNativeWebSigninTrackerBridge = null;
    }

    @Test
    public void testFactoryCreateDoesNotStart() {
        mFactory.createWithCoreAccountId(mProfileMock, CORE_ACCOUNT_INFO.getId(), mCallbackMock);

        verify(mNativeMock, never()).start(any());
    }

    @Test
    public void testStart() {
        WebSigninTrackerBridge webSigninTrackerBridge =
                mFactory.createWithCoreAccountId(
                        mProfileMock, CORE_ACCOUNT_INFO.getId(), mCallbackMock);

        webSigninTrackerBridge.start();

        verify(mNativeMock).start(mNativeWebSigninTrackerBridge);
    }

    @Test
    public void testResultReportedSynchronouslyFromStart() {
        when(mNativeMock.createWithCoreAccountId(
                        eq(mProfileMock),
                        eq(CORE_ACCOUNT_INFO.getId()),
                        mWebSigninCallbackCaptor.capture()))
                .thenReturn(mNativeWebSigninTrackerBridge);
        // Native reports the result from start() if it's already known, e.g. if the account is
        // already in cookies.
        doAnswer(
                        invocation -> {
                            mWebSigninCallbackCaptor
                                    .getValue()
                                    .onResult(WebSigninTrackerResult.SUCCESS);
                            return null;
                        })
                .when(mNativeMock)
                .start(mNativeWebSigninTrackerBridge);
        WebSigninTrackerBridge webSigninTrackerBridge =
                mFactory.createWithCoreAccountId(
                        mProfileMock, CORE_ACCOUNT_INFO.getId(), mCallbackMock);

        webSigninTrackerBridge.start();

        verify(mCallbackMock).onResult(WebSigninTrackerResult.SUCCESS);
        mNativeWebSigninTrackerBridge = null;
    }

    @Test
    public void testOnSigninSucceed() {
        WebSigninTrackerBridge.onSigninResult(mCallbackMock, WebSigninTrackerResult.SUCCESS);
        verify(mCallbackMock).onResult(WebSigninTrackerResult.SUCCESS);
    }

    @Test
    public void testOnSigninFailed() {
        WebSigninTrackerBridge.onSigninResult(mCallbackMock, WebSigninTrackerResult.OTHER_ERROR);
        verify(mCallbackMock).onResult(WebSigninTrackerResult.OTHER_ERROR);
    }
}
