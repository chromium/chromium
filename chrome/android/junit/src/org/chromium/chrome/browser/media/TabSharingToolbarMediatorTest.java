// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Unit tests for {@link TabSharingToolbarMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSharingToolbarMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabSharingUiBridge mBridge;
    private WebContents mCapturer;
    @Mock private WebContents mCapturee;
    @Mock private WebContents mOtherWebContents;
    @Mock private Tab mCurrentTab;
    @Mock private UrlFormatter.Natives mUrlFormatterJniMock;
    @Mock private MediaCaptureDevicesDispatcherAndroid.Natives mDispatcherJniMock;

    private final ActivityTabProvider mTabProvider = new ActivityTabProvider();
    private Context mContext;
    private PropertyModel mModel;
    private TabSharingToolbarMediator mMediator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mCapturer =
                Mockito.mock(
                        WebContents.class,
                        Mockito.withSettings()
                                .extraInterfaces(WebContentsObserver.Observable.class));
        when(mBridge.getCapturer()).thenReturn(mCapturer);
        when(mBridge.getCapturee()).thenReturn(mCapturee);
        when(mBridge.isSourceSwitchingSupported()).thenReturn(true);
        when(mBridge.appPreferredCurrentTab()).thenReturn(false);
        GURL capturerUrl = new GURL("https://meet.google.com");
        GURL captureeUrl = new GURL("https://youtube.com");
        GURL otherUrl = new GURL("https://other.com");
        when(mCapturer.getLastCommittedUrl()).thenReturn(capturerUrl);
        when(mCapturee.getLastCommittedUrl()).thenReturn(captureeUrl);
        when(mCurrentTab.getUrl()).thenReturn(otherUrl);
        when(mCurrentTab.isNativePage()).thenReturn(false);
        when(mCurrentTab.getWebContents()).thenReturn(mOtherWebContents);
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

        MediaCaptureDevicesDispatcherAndroidJni.setInstanceForTesting(mDispatcherJniMock);
        when(mDispatcherJniMock.shouldFilterWebContents(any(), any())).thenReturn(false);

        mModel = new PropertyModel.Builder(TabSharingToolbarProperties.ALL_KEYS).build();
    }

    @After
    public void tearDown() {
        if (mMediator != null) {
            mMediator.destroy();
        }
        if (mCapturer != null) {
            MediaCaptureDevicesDispatcherAndroid.setSourceSwitchingInProgress(mCapturer, false);
        }
        UrlFormatterJni.setInstanceForTesting(null);
        MediaCaptureDevicesDispatcherAndroidJni.setInstanceForTesting(null);
    }

    @Test
    public void testStatus_ViewingCaptureeTab() {
        when(mCurrentTab.getWebContents()).thenReturn(mCapturee);
        mMediator = new TabSharingToolbarMediator(mContext, mModel, mBridge, mTabProvider);

        CharSequence status = mModel.get(TabSharingToolbarProperties.STATUS_TEXT);
        assertNotNull(status);
        assertTrue(status.toString().contains("Sharing this tab to meet.google.com"));
        assertFalse(mModel.get(TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE));
    }

    @Test
    public void testStatus_ViewingCapturerTab() {
        when(mCurrentTab.getWebContents()).thenReturn(mCapturer);
        mMediator = new TabSharingToolbarMediator(mContext, mModel, mBridge, mTabProvider);

        CharSequence status = mModel.get(TabSharingToolbarProperties.STATUS_TEXT);
        assertNotNull(status);
        assertTrue(status.toString().contains("Sharing youtube.com to this tab"));
        assertFalse(mModel.get(TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE));
    }

    @Test
    public void testStatus_ViewingOtherTab() {
        when(mCurrentTab.getWebContents()).thenReturn(mOtherWebContents);
        mMediator = new TabSharingToolbarMediator(mContext, mModel, mBridge, mTabProvider);

        CharSequence status = mModel.get(TabSharingToolbarProperties.STATUS_TEXT);
        assertNotNull(status);
        assertTrue(status.toString().contains("Sharing youtube.com to meet.google.com"));
        assertTrue(mModel.get(TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE));
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

    @Test
    public void testShareInsteadButton() {
        when(mCurrentTab.getWebContents()).thenReturn(mOtherWebContents);
        mMediator = new TabSharingToolbarMediator(mContext, mModel, mBridge, mTabProvider);

        Runnable listener = mModel.get(TabSharingToolbarProperties.SHARE_INSTEAD_CLICK_LISTENER);
        assertNotNull(listener);
        listener.run();
        verify(mBridge).changeSource(mOtherWebContents);
    }

    @Test
    public void testShareInsteadButton_DebouncedWhenSwitchInProgress() {
        when(mCurrentTab.getWebContents()).thenReturn(mOtherWebContents);
        mMediator = new TabSharingToolbarMediator(mContext, mModel, mBridge, mTabProvider);

        MediaCaptureDevicesDispatcherAndroid.setSourceSwitchingInProgress(mCapturer, true);
        Runnable listener = mModel.get(TabSharingToolbarProperties.SHARE_INSTEAD_CLICK_LISTENER);
        assertNotNull(listener);
        listener.run();
        verify(mBridge, Mockito.never()).changeSource(any());

        MediaCaptureDevicesDispatcherAndroid.setSourceSwitchingInProgress(mCapturer, false);
        listener.run();
        verify(mBridge).changeSource(mOtherWebContents);
    }

    @Test
    public void testDynamicTabSwitchingObserver() {
        when(mCurrentTab.getWebContents()).thenReturn(mCapturee);
        mMediator = new TabSharingToolbarMediator(mContext, mModel, mBridge, mTabProvider);
        assertTrue(
                mModel.get(TabSharingToolbarProperties.STATUS_TEXT)
                        .toString()
                        .contains("meet.google.com"));

        // Simulate switching active tabs dynamically
        Tab newTab = Mockito.mock(Tab.class);
        when(newTab.getWebContents()).thenReturn(mCapturer);
        when(newTab.isNativePage()).thenReturn(false);
        when(newTab.getUrl()).thenReturn(new GURL("https://meet.google.com"));
        mTabProvider.setForTesting(newTab);
        assertTrue(
                mModel.get(TabSharingToolbarProperties.STATUS_TEXT)
                        .toString()
                        .contains("youtube.com to this tab"));
        assertFalse(mModel.get(TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE));

        mTabProvider.setForTesting(null);
        assertFalse(mModel.get(TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE));
    }

    @Test
    public void testSourceSwitchingNotSupported() {
        when(mBridge.isSourceSwitchingSupported()).thenReturn(false);
        when(mCurrentTab.getWebContents()).thenReturn(mOtherWebContents);
        mModel.set(TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE, true);
        mMediator = new TabSharingToolbarMediator(mContext, mModel, mBridge, mTabProvider);
        assertFalse(mModel.get(TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE));
    }

    @Test
    public void testAppPreferredCurrentTabOnCapturer() {
        when(mBridge.appPreferredCurrentTab()).thenReturn(true);
        when(mCurrentTab.getWebContents()).thenReturn(mCapturer);
        mMediator = new TabSharingToolbarMediator(mContext, mModel, mBridge, mTabProvider);
        assertTrue(mModel.get(TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE));
    }

    @Test
    public void testUnpickableTabs_NativePageAndExclusions() {
        when(mCurrentTab.getWebContents()).thenReturn(mOtherWebContents);
        when(mCurrentTab.isNativePage()).thenReturn(true);
        mMediator = new TabSharingToolbarMediator(mContext, mModel, mBridge, mTabProvider);
        assertFalse(mModel.get(TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE));

        when(mCurrentTab.isNativePage()).thenReturn(false);
        when(mCurrentTab.getUrl()).thenReturn(new GURL("chrome://newtab"));
        mMediator = new TabSharingToolbarMediator(mContext, mModel, mBridge, mTabProvider);
        assertFalse(mModel.get(TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE));

        when(mCurrentTab.getUrl()).thenReturn(new GURL("https://other.com"));
        when(mDispatcherJniMock.shouldFilterWebContents(any(), any())).thenReturn(true);
        mMediator = new TabSharingToolbarMediator(mContext, mModel, mBridge, mTabProvider);
        assertFalse(mModel.get(TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE));
    }

    @Test
    public void testNavigationAndContentChangeUpdatesButtonVisibility() {
        when(mCurrentTab.getWebContents()).thenReturn(null);
        when(mCurrentTab.isNativePage()).thenReturn(true);
        mMediator = new TabSharingToolbarMediator(mContext, mModel, mBridge, mTabProvider);
        assertFalse(mModel.get(TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE));

        ArgumentCaptor<TabObserver> observerCaptor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mCurrentTab).addObserver(observerCaptor.capture());
        TabObserver tabObserver = observerCaptor.getValue();

        when(mCurrentTab.isNativePage()).thenReturn(false);
        when(mCurrentTab.getWebContents()).thenReturn(mOtherWebContents);
        tabObserver.onContentChanged(mCurrentTab);
        tabObserver.onUrlUpdated(mCurrentTab);
        tabObserver.onPageLoadFinished(mCurrentTab, new GURL("https://wikipedia.org"));
        assertTrue(mModel.get(TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE));

        when(mCurrentTab.getWebContents()).thenReturn(null);
        tabObserver.onContentChanged(mCurrentTab);
        assertFalse(mModel.get(TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE));
    }

    @Test
    public void testClickableSpanExecution() {
        when(mCurrentTab.getWebContents()).thenReturn(mCapturee);
        mMediator = new TabSharingToolbarMediator(mContext, mModel, mBridge, mTabProvider);
        CharSequence status = mModel.get(TabSharingToolbarProperties.STATUS_TEXT);
        assertTrue(status instanceof Spanned);
        Spanned spanned = (Spanned) status;
        ClickableSpan[] spans = spanned.getSpans(0, spanned.length(), ClickableSpan.class);
        assertTrue(spans.length > 0);

        try {
            View mockView = Mockito.mock(View.class);
            spans[0].onClick(mockView);
        } catch (RuntimeException e) {
            // Expected fallthrough when unmocked JNI boundary in TabImplJni is reached in
            // Robolectric.
        }
        android.text.TextPaint paint = new android.text.TextPaint();
        spans[0].updateDrawState(paint);
        assertFalse(paint.isUnderlineText());
    }
}
