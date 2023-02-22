// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.Intent;

import com.google.common.collect.ImmutableMap;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.feed.FeedActionDelegateImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.share.crow.CrowButtonDelegate;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/** Tests for FeedActionDelegateImpl. */
@RunWith(BaseRobolectricTestRunner.class)
public final class FeedActionDelegateImplTest {
    @Mock
    private SyncConsentActivityLauncher mMockSyncConsentActivityLauncher;

    @Mock
    private SnackbarManager mMockSnackbarManager;

    @Mock
    private NativePageNavigationDelegate mMockNavigationDelegate;

    @Mock
    private BookmarkModel mMockBookmarkModel;

    @Mock
    private Context mActivityContext;

    @Mock
    private CrowButtonDelegate mMockCrowButtonDelegate;

    @Captor
    ArgumentCaptor<Intent> mIntentCaptor;

    private FeedActionDelegateImpl mFeedActionDelegateImpl;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        SyncConsentActivityLauncherImpl.setLauncherForTest(mMockSyncConsentActivityLauncher);
        mFeedActionDelegateImpl = new FeedActionDelegateImpl(mActivityContext, mMockSnackbarManager,
                mMockNavigationDelegate, mMockBookmarkModel, mMockCrowButtonDelegate,
                BrowserUiUtils.HostSurface.NOT_SET);
    }

    @After
    public void tearDown() {
        SyncConsentActivityLauncherImpl.setLauncherForTest(null);
    }

    @Test
    public void testShowSignInActivity_shownWhenFlagEnabled() {
        FeatureList.setTestFeatures(
                ImmutableMap.of(ChromeFeatureList.FEED_SHOW_SIGN_IN_COMMAND, true));
        mFeedActionDelegateImpl.showSignInActivity();
        verify(mMockSyncConsentActivityLauncher)
                .launchActivityIfAllowed(any(), eq(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void testShowSignInActivity_dontShowWhenFlagDisabled() {
        FeatureList.setTestFeatures(
                ImmutableMap.of(ChromeFeatureList.FEED_SHOW_SIGN_IN_COMMAND, false));
        mFeedActionDelegateImpl.showSignInActivity();
        verify(mMockSyncConsentActivityLauncher, never())
                .launchActivityIfAllowed(any(), eq(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void testOpenWebFeed_enabledWhenCormorantFlagEnabled() {
        FeatureList.setTestFeatures(ImmutableMap.of(ChromeFeatureList.CORMORANT, true));
        String webFeedName = "SomeFeedName";

        mFeedActionDelegateImpl.openWebFeed(webFeedName);

        verify(mActivityContext).startActivity(mIntentCaptor.capture());
        Assert.assertArrayEquals("Feed ID not passed correctly.", webFeedName.getBytes(),
                mIntentCaptor.getValue().getByteArrayExtra("CREATOR_WEB_FEED_ID"));
    }

    @Test
    public void testOpenWebFeed_disabledWhenCormorantFlagDisabled() {
        FeatureList.setTestFeatures(ImmutableMap.of(ChromeFeatureList.CORMORANT, false));
        mFeedActionDelegateImpl.openWebFeed("SomeFeedName");
        verify(mActivityContext, never()).startActivity(any());
    }
}
