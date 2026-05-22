// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Intent;

import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.mojom.WindowOpenDisposition;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.feed.FeedActionDelegateImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Tests for FeedActionDelegateImpl. */
@RunWith(BaseRobolectricTestRunner.class)
public final class FeedActionDelegateImplTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SigninAndHistorySyncActivityLauncher mMockSigninAndHistorySyncActivityLauncher;

    @Mock private SnackbarManager mMockSnackbarManager;

    @Mock private NativePageNavigationDelegate mMockNavigationDelegate;

    @Mock private BookmarkModel mMockBookmarkModel;

    @Mock private Activity mActivity;

    @Mock private WindowAndroid mWindowAndroid;

    @Mock private ActivityResultTracker mActivityResultTracker;

    @Mock private DeviceLockActivityLauncher mDeviceLockActivityLauncher;

    @Mock private ModalDialogManager mModalDialogManager;

    @Mock private Profile mProfile;

    @Mock private BottomSheetController mBottomSheetController;

    @Mock private Intent mSigninIntent;

    @Mock private BottomSheetSigninAndHistorySyncCoordinator mSigninCoordinator;

    @Mock private FeedActionDelegate.PageLoadObserver mMockPageLoadObserver;

    @Captor ArgumentCaptor<Intent> mIntentCaptor;

    private FeedActionDelegateImpl mFeedActionDelegateImpl;

    @Before
    public void setUp() {

        SigninAndHistorySyncActivityLauncherImpl.setLauncherForTest(
                mMockSigninAndHistorySyncActivityLauncher);
        mFeedActionDelegateImpl = buildFeedActionDelegateImpl();
    }

    @Test
    @DisableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testStartSigninFlow_shownWhenFlagEnabled() {
        when(mActivity.getString(anyInt())).thenReturn("string");
        when(mMockSigninAndHistorySyncActivityLauncher.createBottomSheetSigninIntentOrShowError(
                        any(), any(), any(), eq(SigninAccessPoint.NTP_FEED_TOP_PROMO)))
                .thenReturn(mSigninIntent);

        mFeedActionDelegateImpl.startSigninFlow(SigninAccessPoint.NTP_FEED_TOP_PROMO);

        ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> configCaptor =
                ArgumentCaptor.forClass(BottomSheetSigninAndHistorySyncConfig.class);
        verify(mMockSigninAndHistorySyncActivityLauncher)
                .createBottomSheetSigninIntentOrShowError(
                        any(),
                        any(),
                        configCaptor.capture(),
                        eq(SigninAccessPoint.NTP_FEED_TOP_PROMO));
        BottomSheetSigninAndHistorySyncConfig config = configCaptor.getValue();
        assertEquals(NoAccountSigninMode.BOTTOM_SHEET, config.noAccountSigninMode);
        assertEquals(
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET, config.withAccountSigninMode);
        assertEquals(HistorySyncConfig.OptInMode.NONE, config.historyOptInMode);
        assertNull(config.selectedCoreAccountId);
        verify(mActivity).startActivity(mSigninIntent);
    }

    @Test
    @DisableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testShowSigninInterstitial() {
        when(mActivity.getString(anyInt())).thenReturn("string");
        when(mMockSigninAndHistorySyncActivityLauncher.createBottomSheetSigninIntentOrShowError(
                        any(), any(), any(), eq(SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO)))
                .thenReturn(mSigninIntent);
        mFeedActionDelegateImpl.showSignInInterstitial(
                SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO, null);

        ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> configCaptor =
                ArgumentCaptor.forClass(BottomSheetSigninAndHistorySyncConfig.class);
        verify(mMockSigninAndHistorySyncActivityLauncher)
                .createBottomSheetSigninIntentOrShowError(
                        any(),
                        any(),
                        configCaptor.capture(),
                        eq(SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO));
        BottomSheetSigninAndHistorySyncConfig config = configCaptor.getValue();
        assertEquals(NoAccountSigninMode.BOTTOM_SHEET, config.noAccountSigninMode);
        assertEquals(
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET, config.withAccountSigninMode);
        assertEquals(HistorySyncConfig.OptInMode.NONE, config.historyOptInMode);
        assertNull(config.selectedCoreAccountId);
        verify(mActivity).startActivity(mSigninIntent);
    }

    @Test
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testStartSigninFlow_seamlessEnabled() {
        when(mMockSigninAndHistorySyncActivityLauncher
                        .createBottomSheetSigninCoordinatorAndObserveAddAccountResult(
                                any(), any(), any(), any(), any(), any(), any(), any(), any(),
                                anyInt()))
                .thenReturn(mSigninCoordinator);
        mFeedActionDelegateImpl = buildFeedActionDelegateImpl();

        when(mActivity.getString(anyInt())).thenReturn("string");

        mFeedActionDelegateImpl.startSigninFlow(SigninAccessPoint.NTP_FEED_BOTTOM_PROMO);

        verify(mSigninCoordinator).startSigninFlow(any());
    }

    @Test
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testShowSigninInterstitial_seamlessEnabled() {
        when(mMockSigninAndHistorySyncActivityLauncher
                        .createBottomSheetSigninCoordinatorAndObserveAddAccountResult(
                                any(), any(), any(), any(), any(), any(), any(), any(), any(),
                                anyInt()))
                .thenReturn(mSigninCoordinator);
        mFeedActionDelegateImpl = buildFeedActionDelegateImpl();

        when(mActivity.getString(anyInt())).thenReturn("string");

        mFeedActionDelegateImpl.showSignInInterstitial(
                SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO, null);

        verify(mSigninCoordinator).startSigninFlow(any());
    }

    @Test
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testOpenSuggestionUrl_adClickRendererInitiated() {
        String url = "https://google.com/aclk?ad=1";
        LoadUrlParams params = new LoadUrlParams(url);
        mFeedActionDelegateImpl.openSuggestionUrl(
                WindowOpenDisposition.CURRENT_TAB,
                params,
                false,
                1,
                mMockPageLoadObserver,
                0);
        assertTrue(params.getIsRendererInitiated());
        assertTrue(params.getHasUserGesture());
        assertTrue(params.getInitiatorOrigin().isOpaque());
    }

    @Test
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testOpenSuggestionUrl_organicClickNotRendererInitiated() {
        LoadUrlParams params = new LoadUrlParams("https://google.com/search?q=cat");
        mFeedActionDelegateImpl.openSuggestionUrl(
                WindowOpenDisposition.CURRENT_TAB,
                params,
                false,
                1,
                mMockPageLoadObserver,
                0);
        assertFalse(params.getIsRendererInitiated());
        assertFalse(params.getHasUserGesture());
        assertNull(params.getInitiatorOrigin());
    }

    @Test
    @EnableFeatures({
            SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
            SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testOpenSuggestionUrl_aclkPathNonGoogleHostNotAd() {
        LoadUrlParams params = new LoadUrlParams("https://example.com/aclk?ad=1");
        mFeedActionDelegateImpl.openSuggestionUrl(
                WindowOpenDisposition.CURRENT_TAB,
                params,
                false,
                1,
                mMockPageLoadObserver,
                0);
        assertFalse(params.getIsRendererInitiated());
        assertFalse(params.getHasUserGesture());
        assertNull(params.getInitiatorOrigin());
    }

    @Test
    @EnableFeatures({
            SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
            SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testOpenSuggestionUrl_googleHostNonAclkPathNotAd() {
        LoadUrlParams params = new LoadUrlParams("https://google.com/search?q=aclk");
        mFeedActionDelegateImpl.openSuggestionUrl(
                WindowOpenDisposition.CURRENT_TAB,
                params,
                false,
                1,
                mMockPageLoadObserver,
                0);
        assertFalse(params.getIsRendererInitiated());
        assertFalse(params.getHasUserGesture());
        assertNull(params.getInitiatorOrigin());
    }

    @Test
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testOpenUrl_adClickRendererInitiated() {
        String url = "https://google.com/aclk?ad=1";
        LoadUrlParams params = new LoadUrlParams(url);
        mFeedActionDelegateImpl.openUrl(WindowOpenDisposition.CURRENT_TAB, params);
        assertTrue(params.getIsRendererInitiated());
        assertTrue(params.getHasUserGesture());
        assertTrue(params.getInitiatorOrigin().isOpaque());
    }

    @Test
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testOpenUrl_organicClickNotRendererInitiated() {
        LoadUrlParams params = new LoadUrlParams("https://google.com/search?q=cat");
        mFeedActionDelegateImpl.openUrl(WindowOpenDisposition.CURRENT_TAB, params);
        assertFalse(params.getIsRendererInitiated());
        assertFalse(params.getHasUserGesture());
        assertNull(params.getInitiatorOrigin());
    }

    @Test
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testOpenUrl_aclkPathNonGoogleHostNotAd() {
        LoadUrlParams params = new LoadUrlParams("https://example.com/aclk?ad=1");
        mFeedActionDelegateImpl.openUrl(WindowOpenDisposition.CURRENT_TAB, params);
        assertFalse(params.getIsRendererInitiated());
        assertFalse(params.getHasUserGesture());
        assertNull(params.getInitiatorOrigin());
    }

    @Test
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testOpenUrl_googleHostNonAclkPathNotAd() {
        LoadUrlParams params = new LoadUrlParams("https://google.com/search?q=aclk");
        mFeedActionDelegateImpl.openUrl(WindowOpenDisposition.CURRENT_TAB, params);
        assertFalse(params.getIsRendererInitiated());
        assertFalse(params.getHasUserGesture());
        assertNull(params.getInitiatorOrigin());
    }

    private FeedActionDelegateImpl buildFeedActionDelegateImpl() {
        return new FeedActionDelegateImpl(
                mActivity,
                mWindowAndroid,
                mActivityResultTracker,
                mMockSigninAndHistorySyncActivityLauncher,
                mDeviceLockActivityLauncher,
                mMockSnackbarManager,
                mModalDialogManager,
                mMockNavigationDelegate,
                mMockBookmarkModel,
                mProfile,
                mBottomSheetController);
    }
}
