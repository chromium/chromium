// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/** Tests {@link SyncConsentActivityLauncherImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SyncConsentActivityLauncherImplTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private SigninManager mSigninManagerMock;

    @Mock private Context mContextMock;

    @Mock private Profile mProfile;

    private final Context mContext = ContextUtils.getApplicationContext();

    @Before
    public void setUp() {
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
    }

    @Test
    public void testLaunchActivityIfAllowedWhenSigninIsAllowed() {
        when(mSigninManagerMock.isSyncOptInAllowed()).thenReturn(true);
        Assert.assertTrue(
                SyncConsentActivityLauncherImpl.get()
                        .launchActivityIfAllowed(mContextMock, SigninAccessPoint.SETTINGS));
        verify(mContextMock).startActivity(notNull());
    }

    @Test
    public void testLaunchActivityIfAllowedWhenSigninIsNotAllowed() {
        when(mSigninManagerMock.isSyncOptInAllowed()).thenReturn(false);
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(false);
        Object toastBeforeCall = ShadowToast.getLatestToast();
        Assert.assertFalse(
                SyncConsentActivityLauncherImpl.get()
                        .launchActivityIfAllowed(mContext, SigninAccessPoint.SETTINGS));
        Object toastAfterCall = ShadowToast.getLatestToast();
        Assert.assertEquals(
                "No new toast should be made during the call!", toastBeforeCall, toastAfterCall);
    }

    @Test
    @DisabledTest(message = "Flaky - crbug/1457280")
    public void testLaunchActivityIfAllowedWhenSigninIsDisabledByPolicy() {
        HistogramWatcher watchSigninDisabledToastShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SyncDisabledNotificationShown",
                        SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO);
        when(mSigninManagerMock.isSyncOptInAllowed()).thenReturn(false);
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        Assert.assertFalse(
                SyncConsentActivityLauncherImpl.get()
                        .launchActivityIfAllowed(
                                mContext, SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO));
        Assert.assertTrue(
                ShadowToast.showedCustomToast(
                        mContext.getResources().getString(R.string.managed_by_your_organization),
                        R.id.toast_text));
        watchSigninDisabledToastShownHistogram.assertExpected();
    }
}
