// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;

import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.GURLUtils;
import org.chromium.net.GURLUtilsJni;
import org.chromium.services.media_session.MediaMetadata;

import java.util.Set;

/**
 * Utility class for holding a Tab and relevant objects for media notification tests.
 */
public class MediaNotificationTestTabHolder {
    @Mock
    UrlFormatter.Natives mUrlFormatterJniMock;
    @Mock
    GURLUtils.Natives mGURLUtilsJniMock;
    @Mock
    WebContents mWebContents;
    @Mock
    MediaSession mMediaSession;
    @Mock
    Tab mTab;

    String mTitle;
    String mUrl;

    MediaSessionTabHelper mMediaSessionTabHelper;

    // Mock LargeIconBridge that always returns false.
    private class TestLargeIconBridge extends LargeIconBridge {
        @Override
        public boolean getLargeIconForUrl(
                final String pageUrl, int desiredSizePx, final LargeIconCallback callback) {
            return false;
        }
    }

    public MediaNotificationTestTabHolder(int tabId, String url, String title, JniMocker mocker) {
        MockitoAnnotations.initMocks(this);
        mocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
        // We don't want this matcher to match the current value of mUrl. Wrapping it in a matcher
        // allows us to match on the updated value of mUrl.
        when(mUrlFormatterJniMock.formatUrlForDisplayOmitSchemeOmitTrivialSubdomains(
                     argThat(urlArg -> urlArg.equals(mUrl))))
                .thenAnswer(invocation -> mUrl);

        mocker.mock(GURLUtilsJni.TEST_HOOKS, mGURLUtilsJniMock);
        when(mGURLUtilsJniMock.getOrigin(argThat(urlArg -> urlArg.equals(mUrl))))
                .thenAnswer(invocation -> mUrl);

        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getId()).thenReturn(tabId);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.getTitle()).thenAnswer(invocation -> mTitle);
        when(mTab.getUrl()).thenAnswer(invocation -> mUrl);

        MediaSessionTabHelper.sOverriddenMediaSession = mMediaSession;
        mMediaSessionTabHelper = new MediaSessionTabHelper(mTab);
        mMediaSessionTabHelper.mLargeIconBridge = new TestLargeIconBridge();

        simulateNavigation(url, false);
        simulateTitleUpdated(title);
    }

    public void simulateTitleUpdated(String title) {
        mTitle = title;
        mMediaSessionTabHelper.mTabObserver.onTitleUpdated(mTab);
    }

    public void simulateFaviconUpdated(Bitmap icon) {
        mMediaSessionTabHelper.mTabObserver.onFaviconUpdated(mTab, icon);
    }

    public void simulateMediaSessionStateChanged(boolean isControllable, boolean isSuspended) {
        mMediaSessionTabHelper.mMediaSessionObserver.mediaSessionStateChanged(
                isControllable, isSuspended);
    }

    public void simulateMediaSessionMetadataChanged(MediaMetadata metadata) {
        mMediaSessionTabHelper.mMediaSessionObserver.mediaSessionMetadataChanged(metadata);
    }

    public void simulateMediaSessionActionsChanged(Set<Integer> actions) {
        mMediaSessionTabHelper.mMediaSessionObserver.mediaSessionActionsChanged(actions);
    }

    public void simulateNavigation(String url, boolean isSameDocument) {
        mUrl = url;

        NavigationHandle navigation = new NavigationHandle(0 /* navigationHandleProxy */, url,
                true /* isInMainFrame */, isSameDocument, false /* isRendererInitiated */);
        mMediaSessionTabHelper.mTabObserver.onDidStartNavigation(mTab, navigation);

        navigation.didFinish(url, false /* isErrorPage */, true /* hasCommitted */,
                false /* isFragmentNavigation */, false /* isDownload */,
                false /* isValidSearchFormUrl */, 0 /* pageTransition */, 0 /* errorCode */,
                200 /* httpStatusCode */);
        mMediaSessionTabHelper.mTabObserver.onDidFinishNavigation(mTab, navigation);
    }
}
