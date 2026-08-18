// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Unit tests for {@link TabSharingToolbarMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSharingToolbarMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabSharingUIBridge mBridge;
    @Mock private WebContents mCapturer;
    @Mock private WebContents mCapturee;
    @Mock private WebContents mOtherWebContents;
    @Mock private Tab mCurrentTab;
    @Mock private UrlFormatter.Natives mUrlFormatterJniMock;

    private final ActivityTabProvider mTabProvider = new ActivityTabProvider();
    private Context mContext;
    private PropertyModel mModel;
    private TabSharingToolbarMediator mMediator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mBridge.getCapturer()).thenReturn(mCapturer);
        when(mBridge.getCapturee()).thenReturn(mCapturee);
        GURL capturerUrl = new GURL("https://meet.google.com");
        GURL captureeUrl = new GURL("https://youtube.com");
        when(mCapturer.getLastCommittedUrl()).thenReturn(capturerUrl);
        when(mCapturee.getLastCommittedUrl()).thenReturn(captureeUrl);
        mTabProvider.setForTesting(mCurrentTab);

        UrlFormatterJni.setInstanceForTesting(mUrlFormatterJniMock);
        when(mUrlFormatterJniMock.formatUrlForSecurityDisplay(any(), anyInt()))
                .thenAnswer(
                        (invocation) -> {
                            GURL url = (GURL) invocation.getArgument(0);
                            if (url == capturerUrl) return "meet.google.com";
                            if (url == captureeUrl) return "youtube.com";
                            return "other.com";
                        });

        mModel = new PropertyModel.Builder(TabSharingToolbarProperties.ALL_KEYS).build();
    }

    @After
    public void tearDown() {
        if (mMediator != null) {
            mMediator.destroy();
        }
        UrlFormatterJni.setInstanceForTesting(null);
    }

    @Test
    public void testStatus_ViewingCaptureeTab() {
        when(mCurrentTab.getWebContents()).thenReturn(mCapturee);
        mMediator = new TabSharingToolbarMediator(mContext, mModel, mBridge, mTabProvider);

        CharSequence status = mModel.get(TabSharingToolbarProperties.STATUS_TEXT);
        assertNotNull(status);
        assertTrue(status.toString().contains("Sharing this tab to meet.google.com"));
    }

    @Test
    public void testStatus_ViewingCapturerTab() {
        when(mCurrentTab.getWebContents()).thenReturn(mCapturer);
        mMediator = new TabSharingToolbarMediator(mContext, mModel, mBridge, mTabProvider);

        CharSequence status = mModel.get(TabSharingToolbarProperties.STATUS_TEXT);
        assertNotNull(status);
        assertTrue(status.toString().contains("Sharing youtube.com to this tab"));
    }

    @Test
    public void testStatus_ViewingOtherTab() {
        when(mCurrentTab.getWebContents()).thenReturn(mOtherWebContents);
        mMediator = new TabSharingToolbarMediator(mContext, mModel, mBridge, mTabProvider);

        CharSequence status = mModel.get(TabSharingToolbarProperties.STATUS_TEXT);
        assertNotNull(status);
        assertTrue(status.toString().contains("Sharing youtube.com to meet.google.com"));
    }

    @Test
    public void testStopSharingButton() {
        when(mCurrentTab.getWebContents()).thenReturn(mCapturee);
        mMediator = new TabSharingToolbarMediator(mContext, mModel, mBridge, mTabProvider);

        Runnable listener = mModel.get(TabSharingToolbarProperties.STOP_SHARING_CLICK_LISTENER);
        assertNotNull(listener);
        listener.run();
        verify(mBridge).stopSharing();
    }
}
