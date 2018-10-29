// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.action;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.support.test.filters.SmallTest;
import android.util.ArrayMap;

import com.google.android.libraries.feed.api.knowncontent.ContentMetadata;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.feed.FeedLoggingBridge;
import org.chromium.chrome.browser.feed.FeedOfflineIndicator;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
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

    @Captor
    private ArgumentCaptor<Integer> mDispositionCapture;
    @Captor
    private ArgumentCaptor<LoadUrlParams> mLoadParamsCapture;
    @Captor
    private ArgumentCaptor<Long> mOfflineIdCapture;
    @Captor
    private ArgumentCaptor<Callback<LoadUrlParams>> mLoadUrlParamsCallbackCapture;

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
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActionHandler = new FeedActionHandler(mDelegate, mSuggestionConsumedObserver,
                mOfflineIndicator, mOfflinePageBridge, mLoggingBridge);

        doAnswer(invocation -> {
            LoadUrlParams params = new LoadUrlParams("");
            params.setExtraHeaders(Collections.singletonMap("", OFFLINE_ID.toString()));
            mLoadUrlParamsCallbackCapture.getValue().onResult(params);
            return null;
        })
                .when(mOfflinePageBridge)
                .getLoadUrlParamsByOfflineId(mOfflineIdCapture.capture(), anyInt(),
                        mLoadUrlParamsCallbackCapture.capture());
        doNothing().when(mLoggingBridge).onContentTargetVisited(anyLong());

        Map<String, Boolean> featureMap = new ArrayMap<>();
        featureMap.put(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, true);
        ChromeFeatureList.setTestFeatures(featureMap);
    }

    @After
    public void tearDown() {
        ChromeFeatureList.setTestFeatures(null);
    }

    @Test
    @SmallTest
    public void testOpenUrlOnline() {
        when(mOfflineIndicator.getOfflineIdIfPageIsOfflined(TEST_URL)).thenReturn(OFFLINE_ID);
        mActionHandler.openUrl(TEST_URL);
        verifyOpenedOffline(WindowOpenDisposition.CURRENT_TAB);
    }

    @Test
    @SmallTest
    public void testOpenUrlOffline() {
        when(mOfflineIndicator.getOfflineIdIfPageIsOfflined(TEST_URL)).thenReturn(null);
        mActionHandler.openUrl(TEST_URL);
        verifyOpenedOnline(WindowOpenDisposition.CURRENT_TAB);
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
    public void testOpenUrlInNewTabOnline() {
        when(mOfflineIndicator.getOfflineIdIfPageIsOfflined(TEST_URL)).thenReturn(OFFLINE_ID);
        mActionHandler.openUrlInNewTab(TEST_URL);
        verifyOpenedOffline(WindowOpenDisposition.NEW_BACKGROUND_TAB);
    }

    @Test
    @SmallTest
    public void testOpenUrlInNewTabOffline() {
        when(mOfflineIndicator.getOfflineIdIfPageIsOfflined(TEST_URL)).thenReturn(null);
        mActionHandler.openUrlInNewTab(TEST_URL);
        verifyOpenedOnline(WindowOpenDisposition.NEW_BACKGROUND_TAB);
    }

    @Test
    @SmallTest
    public void testOpenUrlInNewWindowOnline() {
        when(mOfflineIndicator.getOfflineIdIfPageIsOfflined(TEST_URL)).thenReturn(OFFLINE_ID);
        mActionHandler.openUrlInNewWindow(TEST_URL);
        verifyOpenedOffline(WindowOpenDisposition.NEW_WINDOW);
    }

    @Test
    @SmallTest
    public void testOpenUrlInNewWindowOffline() {
        when(mOfflineIndicator.getOfflineIdIfPageIsOfflined(TEST_URL)).thenReturn(null);
        mActionHandler.openUrlInNewWindow(TEST_URL);
        verifyOpenedOnline(WindowOpenDisposition.NEW_WINDOW);
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
}
