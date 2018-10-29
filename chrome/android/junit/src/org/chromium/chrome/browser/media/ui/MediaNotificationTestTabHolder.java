// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.mockito.Mockito.when;

import android.graphics.Bitmap;

import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.MediaMetadata;

import java.util.Set;

/**
 * Utility class for holding a Tab and relevant objects for media notification tests.
 */
public class MediaNotificationTestTabHolder {
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

    public MediaNotificationTestTabHolder(int tabId, String url, String title) {
        MockitoAnnotations.initMocks(this);

        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getId()).thenReturn(tabId);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.getTitle()).thenAnswer(new Answer<String>() {
            @Override
            public String answer(InvocationOnMock invocation) {
                return mTitle;
            }
        });
        when(mTab.getUrl()).thenAnswer(new Answer<String>() {
            @Override
            public String answer(InvocationOnMock invocation) {
                return mUrl;
            }
        });

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
        mMediaSessionTabHelper.mTabObserver.onDidFinishNavigation(mTab, url,
                true /* isInMainFrame */, false /* isErrorPage */, true /* hasCommitted */,
                isSameDocument, false /* isFragmentNavigation */, 0 /* pageTransition */,
                0 /* errorCode */, 200 /* httpStatusCode */);
    }
}
