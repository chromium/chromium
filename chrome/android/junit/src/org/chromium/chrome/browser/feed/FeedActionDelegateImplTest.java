// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Intent;

import com.google.common.collect.ImmutableMap;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.app.feed.FeedActionDelegateImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridgeJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/** Tests for FeedActionDelegateImpl. */
@RunWith(BaseRobolectricTestRunner.class)
public final class FeedActionDelegateImplTest {
    @Rule public JniMocker jniMocker = new JniMocker();

    @Mock private WebFeedBridge.Natives mWebFeedBridgeJniMock;

    @Mock private SyncConsentActivityLauncher mMockSyncConsentActivityLauncher;
    @Mock private SigninAndHistorySyncActivityLauncher mMockSigninLauncher;

    @Mock private SigninAndHistorySyncActivityLauncher mMockSigninAndHistorySyncActivityLauncher;

    @Mock private SnackbarManager mMockSnackbarManager;

    @Mock private NativePageNavigationDelegate mMockNavigationDelegate;

    @Mock private BookmarkModel mMockBookmarkModel;

    @Mock private Activity mActivity;

    @Mock private TabModelSelector mTabModelSelector;

    @Mock private Profile mProfile;

    @Mock private BottomSheetController mBottomSheetController;

    @Captor ArgumentCaptor<Intent> mIntentCaptor;

    private FeedActionDelegateImpl mFeedActionDelegateImpl;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        SyncConsentActivityLauncherImpl.setLauncherForTest(mMockSyncConsentActivityLauncher);
        SigninAndHistorySyncActivityLauncherImpl.setLauncherForTest(
                mMockSigninAndHistorySyncActivityLauncher);
        mFeedActionDelegateImpl =
                new FeedActionDelegateImpl(
                        mActivity,
                        mMockSnackbarManager,
                        mMockNavigationDelegate,
                        mMockBookmarkModel,
                        mTabModelSelector,
                        mProfile,
                        mBottomSheetController);
        jniMocker.mock(WebFeedBridgeJni.TEST_HOOKS, mWebFeedBridgeJniMock);

        when(mWebFeedBridgeJniMock.isCormorantEnabledForLocale()).thenReturn(true);
    }

    @Test
    public void testShowSyncConsentActivity_shownWhenFlagEnabled() {
        FeatureList.setTestFeatures(
                ImmutableMap.of(ChromeFeatureList.FEED_SHOW_SIGN_IN_COMMAND, true));
        mFeedActionDelegateImpl.showSyncConsentActivity(SigninAccessPoint.NTP_FEED_TOP_PROMO);
        verify(mMockSyncConsentActivityLauncher)
                .launchActivityIfAllowed(any(), eq(SigninAccessPoint.NTP_FEED_TOP_PROMO));
    }

    @Test
    public void testShowSyncConsentActivity_dontShowWhenFlagDisabled() {
        FeatureList.setTestFeatures(
                ImmutableMap.of(ChromeFeatureList.FEED_SHOW_SIGN_IN_COMMAND, false));
        mFeedActionDelegateImpl.showSyncConsentActivity(SigninAccessPoint.NTP_FEED_TOP_PROMO);
        verify(mMockSyncConsentActivityLauncher, never())
                .launchActivityIfAllowed(any(), eq(SigninAccessPoint.NTP_FEED_TOP_PROMO));
    }

    @Test
    public void testStartSigninFlow_shownWhenFlagEnabled() {
        FeatureList.setTestFeatures(
                ImmutableMap.of(ChromeFeatureList.FEED_SHOW_SIGN_IN_COMMAND, true));
        mFeedActionDelegateImpl.startSigninFlow(SigninAccessPoint.NTP_FEED_TOP_PROMO);
        verify(mMockSigninAndHistorySyncActivityLauncher)
                .launchActivityIfAllowed(
                        any(),
                        any(),
                        any(),
                        eq(SigninAndHistorySyncCoordinator.NoAccountSigninMode.BOTTOM_SHEET),
                        eq(
                                SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                        .DEFAULT_ACCOUNT_BOTTOM_SHEET),
                        eq(SigninAndHistorySyncCoordinator.HistoryOptInMode.NONE),
                        eq(SigninAccessPoint.NTP_FEED_TOP_PROMO));
    }

    @Test
    public void testStartSigninFlow_dontShowWhenFlagDisabled() {
        FeatureList.setTestFeatures(
                ImmutableMap.of(ChromeFeatureList.FEED_SHOW_SIGN_IN_COMMAND, false));
        mFeedActionDelegateImpl.startSigninFlow(SigninAccessPoint.NTP_FEED_TOP_PROMO);
        verify(mMockSigninAndHistorySyncActivityLauncher, never())
                .launchActivityIfAllowed(
                        any(),
                        any(),
                        any(),
                        eq(SigninAndHistorySyncCoordinator.NoAccountSigninMode.BOTTOM_SHEET),
                        eq(
                                SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                        .DEFAULT_ACCOUNT_BOTTOM_SHEET),
                        eq(SigninAndHistorySyncCoordinator.HistoryOptInMode.NONE),
                        eq(SigninAccessPoint.NTP_FEED_TOP_PROMO));
    }

    @Test
    public void testShowSigninInterstitial_replaceSyncPromosWithSignInPromosEnabled() {
        FeatureList.setTestFeatures(
                ImmutableMap.of(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS, true));
        mFeedActionDelegateImpl.showSignInInterstitial(
                SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO, null, null);
        verify(mMockSigninAndHistorySyncActivityLauncher)
                .launchActivityIfAllowed(
                        any(),
                        any(),
                        any(),
                        eq(SigninAndHistorySyncCoordinator.NoAccountSigninMode.BOTTOM_SHEET),
                        eq(
                                SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                        .DEFAULT_ACCOUNT_BOTTOM_SHEET),
                        eq(SigninAndHistorySyncCoordinator.HistoryOptInMode.NONE),
                        eq(SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO));
    }

    @Test
    public void testOpenWebFeed_enabledWhenCormorantFlagEnabled() {
        FeatureList.setTestFeatures(ImmutableMap.of(ChromeFeatureList.CORMORANT, true));
        String webFeedName = "SomeFeedName";

        mFeedActionDelegateImpl.openWebFeed(webFeedName, SingleWebFeedEntryPoint.OTHER);

        verify(mActivity).startActivity(mIntentCaptor.capture());
        Assert.assertArrayEquals(
                "Feed ID not passed correctly.",
                webFeedName.getBytes(),
                mIntentCaptor.getValue().getByteArrayExtra("CREATOR_WEB_FEED_ID"));
    }

    @Test
    public void testOpenWebFeed_disabledWhenCormorantFlagDisabled() {
        when(mWebFeedBridgeJniMock.isCormorantEnabledForLocale()).thenReturn(false);
        FeatureList.setTestFeatures(ImmutableMap.of(ChromeFeatureList.CORMORANT, false));
        mFeedActionDelegateImpl.openWebFeed("SomeFeedName", SingleWebFeedEntryPoint.OTHER);
        verify(mActivity, never()).startActivity(any());
    }
}
