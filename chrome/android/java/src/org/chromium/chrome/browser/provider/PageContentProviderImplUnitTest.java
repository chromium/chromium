// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.database.ContentObserver;
import android.database.Cursor;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowContentResolver;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.content_extraction.InnerTextBridge;
import org.chromium.chrome.browser.content_extraction.InnerTextBridgeJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.JUnitTestGURLs;

import java.util.Optional;
import java.util.concurrent.TimeoutException;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.PAGE_CONTENT_PROVIDER})
@Config(
        manifest = Config.NONE,
        shadows = {ShadowContentResolver.class})
public class PageContentProviderImplUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock private RenderFrameHost mRenderFrameHost;
    @Mock private WebContents mWebContents;
    @Mock private Tab mTab;
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private InnerTextBridge.Natives mInnerTextNatives;
    private PageContentProvider mProvider;

    @Before
    public void setUp() throws Exception {
        PageContentProviderImpl.clearCachedContent();
        mProvider = new PageContentProvider();

        InnerTextBridgeJni.setInstanceForTesting(mInnerTextNatives);
        when(mWebContents.getMainFrame()).thenReturn(mRenderFrameHost);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL);
        when(mActivityTabProvider.get()).thenReturn(mTab);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testInvalidUrl() {
        mProvider.query(Uri.parse("content://com.android.invalid/123456"), null, null, null, null);
    }

    @Test
    public void testGetContentUrl() {
        var contentUri =
                PageContentProviderImpl.getContentUriForUrl(
                        "https://google.com", mActivityTabProvider);
        assertNotNull(contentUri);
    }

    @Test
    public void testQueryValidContentUrl() {
        var contentUri =
                PageContentProviderImpl.getContentUriForUrl(
                        mTab.getUrl().getSpec(), mActivityTabProvider);
        // Wait 300ms between creating URI and querying it.
        mFakeTimeTestRule.advanceMillis(300);
        Cursor resultCursor;
        // Time between creating URI and querying it should be recorded.
        try (HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.AssistContent.WebPageContentProvider.Latency.CreateToExtractionStart",
                        300)) {
            resultCursor = mProvider.query(Uri.parse(contentUri), null, null, null, null);
        }
        assertCursorContainsValues(
                resultCursor,
                mTab.getUrl().getSpec(),
                /* isFinishedLoading= */ false,
                /* contents= */ "");
        verify(mInnerTextNatives).getInnerText(eq(mRenderFrameHost), any());
    }

    @Test
    public void testObserverUpdate() throws TimeoutException {
        var contentObserverCallbackHelper = new CallbackHelper();
        ArgumentCaptor<Callback<Optional<String>>> innerTextCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);

        var contentUri =
                PageContentProviderImpl.getContentUriForUrl(
                        mTab.getUrl().getSpec(), mActivityTabProvider);
        mFakeTimeTestRule.advanceMillis(250);
        mProvider.query(Uri.parse(contentUri), null, null, null, null);
        verify(mInnerTextNatives).getInnerText(any(), innerTextCallbackCaptor.capture());

        ContextUtils.getApplicationContext()
                .getContentResolver()
                .registerContentObserver(
                        Uri.parse(contentUri),
                        false,
                        new ContentObserver(new Handler(Looper.getMainLooper())) {
                            @Override
                            public void onChange(boolean selfChange) {
                                contentObserverCallbackHelper.notifyCalled();
                            }
                        });

        // Wait 600ms between starting the text extraction process and it completing.
        mFakeTimeTestRule.advanceMillis(600);
        // Time it took to extract text should have been recorded.
        try (var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.AssistContent.WebPageContentProvider.Latency.ExtractionStartToEnd",
                        600)) {
            innerTextCallbackCaptor.getValue().onResult(Optional.of("Inner text of page"));
        }
        shadowOf(Looper.getMainLooper()).idle();

        contentObserverCallbackHelper.waitForCallback(0);
    }

    @Test
    public void testQueryWithLoadedResults() {
        ArgumentCaptor<Callback<Optional<String>>> innerTextCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);

        var contentUri =
                PageContentProviderImpl.getContentUriForUrl(
                        mTab.getUrl().getSpec(), mActivityTabProvider);
        mFakeTimeTestRule.advanceMillis(100);
        mProvider.query(Uri.parse(contentUri), null, null, null, null);
        verify(mInnerTextNatives).getInnerText(any(), innerTextCallbackCaptor.capture());

        mFakeTimeTestRule.advanceMillis(200);
        innerTextCallbackCaptor.getValue().onResult(Optional.of("Inner text of page"));

        mFakeTimeTestRule.advanceMillis(300);
        Cursor secondResultCursor;

        // Time between completing text extraction and second query should be recorded, as well as
        // the total time passed.
        try (var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.AssistContent.WebPageContentProvider.Latency.ExtractionEndToFinalQuery",
                                300)
                        .expectIntRecord(
                                "Android.AssistContent.WebPageContentProvider.Latency.CreateToFinalQuery",
                                100 + 200 + 300)
                        .build()) {
            secondResultCursor = mProvider.query(Uri.parse(contentUri), null, null, null, null);
        }
        assertCursorContainsValues(
                secondResultCursor,
                mTab.getUrl().getSpec(),
                /* isFinishedLoading= */ true,
                /* contents= */ "Inner text of page");
    }

    private void assertCursorContainsValues(
            Cursor cursor, String url, boolean isFinishedLoading, String contents) {
        assertEquals(1, cursor.getCount());
        cursor.moveToFirst();
        assertNotNull(cursor);
        var urlColumnIndex = cursor.getColumnIndex("_id");
        var isLoadedColumnIndex = cursor.getColumnIndex("is_finished_loading");
        var contentsColumnIndex = cursor.getColumnIndex("contents");
        assertNotEquals(-1, urlColumnIndex);
        assertNotEquals(-1, isLoadedColumnIndex);
        assertNotEquals(-1, contentsColumnIndex);

        assertEquals(url, cursor.getString(urlColumnIndex));
        assertEquals(isFinishedLoading ? 1 : 0, cursor.getInt(isLoadedColumnIndex));
        assertEquals(contents, cursor.getString(contentsColumnIndex));
    }
}
