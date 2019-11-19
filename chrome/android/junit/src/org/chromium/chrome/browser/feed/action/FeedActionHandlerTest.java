// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.action;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.support.test.filters.SmallTest;
import android.util.ArrayMap;

import com.google.android.libraries.feed.api.client.knowncontent.ContentMetadata;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.feed.FeedLoggingBridge;
import org.chromium.chrome.browser.feed.FeedOfflineIndicator;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.Collections;
import java.util.Map;

/**
 * Unit tests for {@link FeedActionHandler}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedActionHandlerTest {
    private static final String TEST_URL = "http://www.one.com/";
    private static final Long OFFLINE_ID = 12345L;

    @Mock
    private SuggestionsNavigationDelegate mDelegate;
    @Mock
    private Runnable mSuggestionConsumedObserver;
    @Mock
    private FeedOfflineIndicator mOfflineIndicator;
    @Mock
    private OfflinePageBridge mOfflinePageBridge;
    @Mock
    private FeedLoggingBridge mLoggingBridge;
    @Mock
    private Tab mTab;
    @Mock
    private WebContents mWebContents;
    @Mock
    private NavigationController mNavigationController;

    @Captor
    private ArgumentCaptor<Integer> mDispositionCapture;
    @Captor
    private ArgumentCaptor<LoadUrlParams> mLoadParamsCapture;
    @Captor
    private ArgumentCaptor<Long> mOfflineIdCapture;
    @Captor
    private ArgumentCaptor<Callback<LoadUrlParams>> mLoadUrlParamsCallbackCaptor;
    @Captor
    ArgumentCaptor<WebContentsObserver> mWebContentsObserverCaptor;

    int mLastCommittedEntryIndex;
    private FeedActionHandler mActionHandler;

    private void verifyOpenedOffline(int expectedDisposition) {
        assertEquals(OFFLINE_ID, mOfflineIdCapture.getValue());
        verify(mDelegate, times(1))
                .openUrl(mDispositionCapture.capture(), mLoadParamsCapture.capture());
        assertEquals(expectedDisposition, (int) mDispositionCapture.getValue());
        assertTrue(
                mLoadParamsCapture.getValue().getVerbatimHeaders().contains(OFFLINE_ID.toString()));
        verify(mSuggestionConsumedObserver, times(1)).run();
    }

    private void verifyOpenedOnline(int expectedDisposition) {
        verify(mDelegate, times(1))
                .openUrl(mDispositionCapture.capture(), mLoadParamsCapture.capture());
        assertEquals((int) mDispositionCapture.getValue(), expectedDisposition);
        assertEquals(null, mLoadParamsCapture.getValue().getVerbatimHeaders());
        verify(mSuggestionConsumedObserver, times(1)).run();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActionHandler = new FeedActionHandler(mDelegate, mSuggestionConsumedObserver,
                mOfflineIndicator, mOfflinePageBridge, mLoggingBridge);

        // Setup mocks such that when NavigationRecorder#record is called, it immediately invokes
        // the passed callback.
        when(mDelegate.openUrl(anyInt(), any(LoadUrlParams.class))).thenReturn(mTab);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mNavigationController.getLastCommittedEntryIndex())
                .thenReturn(mLastCommittedEntryIndex++);
        doAnswer((InvocationOnMock invocation) -> {
            mWebContentsObserverCaptor.getValue().navigationEntryCommitted();
            return null;
        })
                .when(mWebContents)
                .addObserver(mWebContentsObserverCaptor.capture());

        Map<String, Boolean> featureMap = new ArrayMap<>();
        featureMap.put(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, true);
        ChromeFeatureList.setTestFeatures(featureMap);
    }

    private void answerWithGoodParams() {
        LoadUrlParams params = new LoadUrlParams("");
        params.setExtraHeaders(Collections.singletonMap("", OFFLINE_ID.toString()));
        answerWithGivenParams(params);
    }

    // Configures mOfflinePageBridge to run the passed callback when getLoadUrlParamsByOfflineId is
    // called. If this isn't setup, the callback will never be invoked.
    private void answerWithGivenParams(LoadUrlParams params) {
        doAnswer(invocation -> {
            mLoadUrlParamsCallbackCaptor.getValue().onResult(params);
            return null;
        })
                .when(mOfflinePageBridge)
                .getLoadUrlParamsByOfflineId(mOfflineIdCapture.capture(), anyInt(),
                        mLoadUrlParamsCallbackCaptor.capture());
    }

    @After
    public void tearDown() {
        ChromeFeatureList.setTestFeatures(null);
    }

    @Test
    @SmallTest
    public void testOpenUrlOffline() {
        when(mOfflineIndicator.getOfflineIdIfPageIsOfflined(TEST_URL)).thenReturn(OFFLINE_ID);
        answerWithGoodParams();
        mActionHandler.openUrl(TEST_URL);
        verifyOpenedOffline(WindowOpenDisposition.CURRENT_TAB);
        verify(mLoggingBridge, times(1))
                .onContentTargetVisited(anyLong(), /*isOffline*/ eq(true), anyBoolean());
    }

    @Test
    @SmallTest
    public void testOpenUrlOnline() {
        when(mOfflineIndicator.getOfflineIdIfPageIsOfflined(TEST_URL)).thenReturn(null);
        mActionHandler.openUrl(TEST_URL);
        verifyOpenedOnline(WindowOpenDisposition.CURRENT_TAB);
        verify(mLoggingBridge, times(1))
                .onContentTargetVisited(anyLong(), /*isOffline*/ eq(false), anyBoolean());
    }

    @Test
    @SmallTest
    public void testOpenUrlOfflineWithNullParams() {
        // The indicator will give an offline id, but the model returns a null LoadUrlParams.
        when(mOfflineIndicator.getOfflineIdIfPageIsOfflined(TEST_URL)).thenReturn(OFFLINE_ID);
        answerWithGivenParams(null);
        mActionHandler.openUrl(TEST_URL);
        verifyOpenedOnline(WindowOpenDisposition.CURRENT_TAB);
        verify(mLoggingBridge, times(1))
                .onContentTargetVisited(anyLong(), /*isOffline*/ eq(false), anyBoolean());
    }

    @Test
    @SmallTest
    public void testOpenUrlInIncognitoModeWithOffline() {
        when(mOfflineIndicator.getOfflineIdIfPageIsOfflined(TEST_URL)).thenReturn(OFFLINE_ID);
        mActionHandler.openUrlInIncognitoMode(TEST_URL);

        // Even though this page has an offlined version, it should not be used because offline
        // pages does not support incognito mode.
        verifyOpenedOnline(WindowOpenDisposition.OFF_THE_RECORD);
    }

    @Test
    @SmallTest
    public void testOpenUrlInNewTabOffline() {
        when(mOfflineIndicator.getOfflineIdIfPageIsOfflined(TEST_URL)).thenReturn(OFFLINE_ID);
        answerWithGoodParams();
        mActionHandler.openUrlInNewTab(TEST_URL);
        verifyOpenedOffline(WindowOpenDisposition.NEW_BACKGROUND_TAB);
        verify(mLoggingBridge, times(1))
                .onContentTargetVisited(anyLong(), /*isOffline*/ eq(true), anyBoolean());
    }

    @Test
    @SmallTest
    public void testOpenUrlInNewTabOnline() {
        when(mOfflineIndicator.getOfflineIdIfPageIsOfflined(TEST_URL)).thenReturn(null);
        mActionHandler.openUrlInNewTab(TEST_URL);
        verifyOpenedOnline(WindowOpenDisposition.NEW_BACKGROUND_TAB);
        verify(mLoggingBridge, times(1))
                .onContentTargetVisited(anyLong(), /*isOffline*/ eq(false), anyBoolean());
    }

    @Test
    @SmallTest
    public void testOpenUrlInNewWindowOffline() {
        when(mOfflineIndicator.getOfflineIdIfPageIsOfflined(TEST_URL)).thenReturn(OFFLINE_ID);
        answerWithGoodParams();
        mActionHandler.openUrlInNewWindow(TEST_URL);
        verifyOpenedOffline(WindowOpenDisposition.NEW_WINDOW);
        verify(mLoggingBridge, times(1))
                .onContentTargetVisited(anyLong(), /*isOffline*/ eq(true), anyBoolean());
    }

    @Test
    @SmallTest
    public void testOpenUrlInNewWindowOnline() {
        when(mOfflineIndicator.getOfflineIdIfPageIsOfflined(TEST_URL)).thenReturn(null);
        mActionHandler.openUrlInNewWindow(TEST_URL);
        verifyOpenedOnline(WindowOpenDisposition.NEW_WINDOW);
        verify(mLoggingBridge, times(1))
                .onContentTargetVisited(anyLong(), /*isOffline*/ eq(false), anyBoolean());
    }

    @Test
    @SmallTest
    public void testDownloadUrlWithOffline() {
        when(mOfflineIndicator.getOfflineIdIfPageIsOfflined(TEST_URL)).thenReturn(OFFLINE_ID);
        ContentMetadata metadata = new ContentMetadata(TEST_URL, "", 0, null, null, null, null);
        mActionHandler.downloadUrl(metadata);

        // Even though this page has an offlined version, this is not a request to open the page,
        // and as such the load params should not be updated with the offline id.
        verifyOpenedOnline(WindowOpenDisposition.SAVE_TO_DISK);
    }

    @Test
    @SmallTest
    public void testOpenUrlNullTab() {
        // If openUrl returns null, then the {@link NavigationRecorder} logic should be skipped.
        reset(mDelegate);
        when(mDelegate.openUrl(anyInt(), any(LoadUrlParams.class))).thenReturn(null);

        when(mOfflineIndicator.getOfflineIdIfPageIsOfflined(TEST_URL)).thenReturn(null);
        mActionHandler.openUrl(TEST_URL);
        verifyOpenedOnline(WindowOpenDisposition.CURRENT_TAB);
        verify(mLoggingBridge, times(0))
                .onContentTargetVisited(anyLong(), anyBoolean(), anyBoolean());
    }
}
