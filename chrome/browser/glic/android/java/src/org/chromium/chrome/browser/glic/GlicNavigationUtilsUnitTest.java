// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Resources;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link GlicNavigationUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicNavigationUtilsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfileMock;
    @Mock private WebContents mWebContentsMock;
    @Mock private WindowAndroid mWindowAndroidMock;
    @Mock private Activity mActivityMock;
    @Mock private Resources mResourcesMock;
    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private AccountManagerFacade mAccountManagerFacadeMock;
    @Mock private SigninAndHistorySyncActivityLauncher mLauncherMock;

    @Before
    public void setUp() {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        AccountManagerFacadeProvider.setInstanceForTests(mAccountManagerFacadeMock);
        GlicNavigationUtils.setLauncher(() -> mLauncherMock);

        when(mWebContentsMock.getTopLevelNativeWindow()).thenReturn(mWindowAndroidMock);
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(mActivityMock));
        when(mActivityMock.getResources()).thenReturn(mResourcesMock);
        when(mResourcesMock.getString(anyInt())).thenReturn("test_string");
        when(mIdentityServicesProviderMock.getIdentityManager(mProfileMock))
                .thenReturn(mIdentityManagerMock);
    }

    @After
    public void tearDown() {
        IdentityServicesProvider.setInstanceForTests(null);
        AccountManagerFacadeProvider.setInstanceForTests(null);
        GlicNavigationUtils.setLauncher(null);
    }

    @Test
    public void testShowSignIn_WithPrimaryAccount_CallsUpdateCredentials() {
        AccountInfo accountInfo = TestAccounts.ACCOUNT1;
        when(mIdentityManagerMock.getPrimaryAccountInfo()).thenReturn(accountInfo);

        GlicNavigationUtils.showSignIn(mProfileMock, mWebContentsMock);

        verify(mAccountManagerFacadeMock)
                .updateCredentials(eq(accountInfo.getId()), eq(mActivityMock), isNull());
        verify(mLauncherMock, never())
                .createBottomSheetSigninIntentOrShowError(any(), any(), any(), anyInt());
    }

    @Test
    public void testShowSignIn_WithoutPrimaryAccount_LaunchesSigninBottomSheet() {
        when(mIdentityManagerMock.getPrimaryAccountInfo()).thenReturn(null);
        Intent intentMock = mock(Intent.class);
        when(mLauncherMock.createBottomSheetSigninIntentOrShowError(
                        eq(mActivityMock),
                        eq(mProfileMock),
                        any(BottomSheetSigninAndHistorySyncConfig.class),
                        eq(SigninAccessPoint.GLIC_LAUNCH_BUTTON)))
                .thenReturn(intentMock);

        GlicNavigationUtils.showSignIn(mProfileMock, mWebContentsMock);

        verify(mLauncherMock)
                .createBottomSheetSigninIntentOrShowError(
                        eq(mActivityMock),
                        eq(mProfileMock),
                        any(BottomSheetSigninAndHistorySyncConfig.class),
                        eq(SigninAccessPoint.GLIC_LAUNCH_BUTTON));
        verify(mActivityMock).startActivity(intentMock);
    }
}
