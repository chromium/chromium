// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Intent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.creator.CreatorActionDelegateImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/** Test suite for {@link CreatorActionDelegateImpl}, especially the sign-in behavior. */
@RunWith(BaseRobolectricTestRunner.class)
public class CreatorActionDelegateImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private SigninAndHistorySyncActivityLauncher mSigninLauncher;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private Activity mActivity;
    @Mock private Profile mProfile;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Intent mSigninIntent;
    @Mock private CreatorCoordinator mCreatorCoordinator;

    @Captor ArgumentCaptor<Intent> mIntentCaptor;

    private CreatorActionDelegateImpl mCreatorActionDelegateImpl;

    @Before
    public void setUp() {
        SigninAndHistorySyncActivityLauncherImpl.setLauncherForTest(mSigninLauncher);
        mCreatorActionDelegateImpl =
                new CreatorActionDelegateImpl(
                        mActivity,
                        mProfile,
                        mSnackbarManager,
                        mCreatorCoordinator,
                        0,
                        mBottomSheetController);
    }

    @Test
    public void testShowSignInInterstitial() {
        @SigninAccessPoint int signinAccessPoint = SigninAccessPoint.NTP_FEED_BOTTOM_PROMO;
        when(mSigninLauncher.createBottomSheetSigninIntentOrShowError(
                        any(), any(), any(), eq(signinAccessPoint)))
                .thenReturn(mSigninIntent);

        mCreatorActionDelegateImpl.showSignInInterstitial(
                signinAccessPoint, mBottomSheetController);

        ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> configCaptor =
                ArgumentCaptor.forClass(BottomSheetSigninAndHistorySyncConfig.class);
        verify(mSigninLauncher)
                .createBottomSheetSigninIntentOrShowError(
                        any(), any(), configCaptor.capture(), eq(signinAccessPoint));
        BottomSheetSigninAndHistorySyncConfig config = configCaptor.getValue();
        assertEquals(NoAccountSigninMode.BOTTOM_SHEET, config.noAccountSigninMode);
        assertEquals(
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET, config.withAccountSigninMode);
        assertEquals(HistorySyncConfig.OptInMode.NONE, config.historyOptInMode);
        assertNull(config.selectedCoreAccountId);
        verify(mActivity).startActivity(mSigninIntent);
    }
}
