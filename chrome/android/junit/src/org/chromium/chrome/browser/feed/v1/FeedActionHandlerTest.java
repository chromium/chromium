// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feed.FeedV1ActionOptions;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.mojom.WindowOpenDisposition;

/**
 * Unit tests for {@link FeedActionHandler}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
public class FeedActionHandlerTest {
    private static final String TEST_URL = "http://www.one.com/";

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Mock
    private SuggestionsNavigationDelegate mDelegate;
    @Mock
    private Runnable mSuggestionConsumedObserver;
    @Mock
    private FeedLoggingBridge mLoggingBridge;
    @Mock
    private TabImpl mTab;
    @Mock
    private WebContents mWebContents;
    @Mock
    private NavigationController mNavigationController;

    @Captor
    private ArgumentCaptor<Integer> mDispositionCapture;
    @Captor
    private ArgumentCaptor<LoadUrlParams> mLoadParamsCapture;
    @Captor
    private ArgumentCaptor<Callback<LoadUrlParams>> mLoadUrlParamsCallbackCaptor;
    @Captor
    ArgumentCaptor<WebContentsObserver> mWebContentsObserverCaptor;

    int mLastCommittedEntryIndex;
    private FeedActionHandler mActionHandler;

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
        mActionHandler = new FeedActionHandler(new FeedV1ActionOptions(), mDelegate,
                mSuggestionConsumedObserver, mLoggingBridge, null, null);

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
    }

    @Test
    @SmallTest
    public void testOpenUrlOnline() {
        mActionHandler.openUrl(TEST_URL);
        verifyOpenedOnline(WindowOpenDisposition.CURRENT_TAB);
        verify(mLoggingBridge, times(1)).onContentTargetVisited(anyLong(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testOpenUrlInIncognitoMode() {
        mActionHandler.openUrlInIncognitoMode(TEST_URL);

        // Even though this page has an offlined version, it should not be used because offline
        // pages does not support incognito mode.
        verifyOpenedOnline(WindowOpenDisposition.OFF_THE_RECORD);
    }

    @Test
    @SmallTest
    public void testOpenUrlInNewTabOnline() {
        mActionHandler.openUrlInNewTab(TEST_URL);
        verifyOpenedOnline(WindowOpenDisposition.NEW_BACKGROUND_TAB);
        verify(mLoggingBridge, times(1)).onContentTargetVisited(anyLong(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testOpenUrlInNewWindowOnline() {
        mActionHandler.openUrlInNewWindow(TEST_URL);
        verifyOpenedOnline(WindowOpenDisposition.NEW_WINDOW);
        verify(mLoggingBridge, times(1)).onContentTargetVisited(anyLong(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testOpenUrlNullTab() {
        // If openUrl returns null, then the {@link NavigationRecorder} logic should be skipped.
        reset(mDelegate);
        when(mDelegate.openUrl(anyInt(), any(LoadUrlParams.class))).thenReturn(null);

        mActionHandler.openUrl(TEST_URL);
        verifyOpenedOnline(WindowOpenDisposition.CURRENT_TAB);
        verify(mLoggingBridge, times(0)).onContentTargetVisited(anyLong(), anyBoolean());
    }
}
