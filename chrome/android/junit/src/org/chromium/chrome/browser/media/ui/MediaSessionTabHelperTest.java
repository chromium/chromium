// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.MediaSessionImpl;
import org.chromium.content.browser.MediaSessionImplJni;
import org.chromium.content_public.browser.test.mock.MockWebContents;

/** Tests for {@link MediaSessionTabHelper} lazy-initialization and cleanup. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MediaSessionTabHelperTest {
    private Tab mTab;
    private MockWebContents mWebContents;
    private MediaSessionImpl.Natives mMediaSessionImplJniMock;
    private MediaSessionImpl mMediaSession;
    private MediaSessionTabHelper mHelper;

    @Before
    public void setUp() {
        mTab = mock(Tab.class);
        mWebContents = mock(MockWebContents.class);
        mMediaSessionImplJniMock = mock(MediaSessionImpl.Natives.class);
        mMediaSession = mock(MediaSessionImpl.class);

        MediaSessionImplJni.setInstanceForTesting(mMediaSessionImplJniMock);
        when(mMediaSessionImplJniMock.getMediaSessionFromWebContents(mWebContents))
                .thenReturn(mMediaSession);
    }

    @Test
    public void testLazyInitialization() {
        // Start with null WebContents (simulating frozen tab)
        when(mTab.getWebContents()).thenReturn(null);

        mHelper = new MediaSessionTabHelper(mTab);

        // Helper should exist, but inner MediaSessionHelper should be null
        assertNotNull(mHelper);
        assertNull(mHelper.mMediaSessionHelper);

        // Simulate WebContents being set (tab restored)
        when(mTab.getWebContents()).thenReturn(mWebContents);
        mHelper.mTabObserver.onContentChanged(mTab);

        // Inner helper should now be initialized
        assertNotNull(mHelper.mMediaSessionHelper);
    }

    @Test
    public void testCleanupOnFreeze() {
        // Start with active WebContents
        when(mTab.getWebContents()).thenReturn(mWebContents);

        mHelper = new MediaSessionTabHelper(mTab);

        // Inner helper should be initialized
        assertNotNull(mHelper.mMediaSessionHelper);

        // Simulate WebContents being cleared (tab frozen)
        when(mTab.getWebContents()).thenReturn(null);
        mHelper.mTabObserver.onContentChanged(mTab);

        // Inner helper should be destroyed and set to null
        assertNull(mHelper.mMediaSessionHelper);
    }
}
