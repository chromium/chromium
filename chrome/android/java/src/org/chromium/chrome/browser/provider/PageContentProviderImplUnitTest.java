// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.database.Cursor;
import android.net.Uri;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowContentResolver;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
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
import java.util.concurrent.TimeUnit;

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
        // In production code PageContentProvider must be called from the binder thread, disable
        // thread checks for these tests.
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
        PageContentProviderImpl.clearCachedContent();
        mProvider = new PageContentProvider();

        InnerTextBridgeJni.setInstanceForTesting(mInnerTextNatives);
        when(mWebContents.getMainFrame()).thenReturn(mRenderFrameHost);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL);
        when(mActivityTabProvider.get()).thenReturn(mTab);
    }

    @Test()
    public void testInvalidUrl() {
        var result =
                mProvider.query(
                        Uri.parse("content://com.android.invalid/123456"), null, null, null, null);

        assertCursorContainsErrorMessage(result, "Invalid URI");
    }

    @Test()
    public void testUrlAfterExpiration() throws InterruptedException {
        var contentUri =
                PageContentProviderImpl.getContentUriForUrl(
                        "https://google.com", mActivityTabProvider);

        // Run all delayed tasks to ensure the URI is expired.
        ShadowLooper.idleMainLooper(1, TimeUnit.HOURS);

        var resultCursor = mProvider.query(Uri.parse(contentUri), null, null, null, null);

        assertCursorContainsErrorMessage(resultCursor, "Invalid ID");
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
        setInnerTextExtractionResult("Page contents!", 200);

        var contentUri =
                PageContentProviderImpl.getContentUriForUrl(
                        mTab.getUrl().getSpec(), mActivityTabProvider);
        // Wait 300ms between creating URI and querying it.
        mFakeTimeTestRule.advanceMillis(300);
        Cursor resultCursor;
        // Time between creating URI and querying it should be recorded. As well as the time spent
        // extracting text and the total time passed.
        try (HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.AssistContent.WebPageContentProvider.Latency.CreateToExtractionStart",
                                300)
                        .expectIntRecord(
                                "Android.AssistContent.WebPageContentProvider.Latency.ExtractionStartToEnd",
                                200)
                        .expectIntRecord(
                                "Android.AssistContent.WebPageContentProvider.Latency.TotalLatency",
                                300 + 200)
                        .build()) {
            resultCursor = mProvider.query(Uri.parse(contentUri), null, null, null, null);
        }
        assertCursorContainsValues(
                resultCursor, mTab.getUrl().getSpec(), /* contents= */ "Page contents!");
        verify(mInnerTextNatives).getInnerText(eq(mRenderFrameHost), any());
    }

    @Test
    public void testQuery_errorWhileExtracting() {
        setInnerTextExtractionError(100);

        var contentUri =
                PageContentProviderImpl.getContentUriForUrl(
                        mTab.getUrl().getSpec(), mActivityTabProvider);
        mFakeTimeTestRule.advanceMillis(300);
        Cursor resultCursor = mProvider.query(Uri.parse(contentUri), null, null, null, null);
        assertCursorContainsErrorMessage(resultCursor, "Error during extraction");
    }

    private void setInnerTextExtractionResult(String result, int resultDelayMs) {
        doAnswer(
                        invocationOnMock -> {
                            Callback<Optional<String>> callback =
                                    (Callback<Optional<String>>)
                                            invocationOnMock.getArgument(1, Callback.class);
                            mFakeTimeTestRule.advanceMillis(resultDelayMs);
                            callback.onResult(Optional.of(result));
                            return null;
                        })
                .when(mInnerTextNatives)
                .getInnerText(eq(mRenderFrameHost), any());
    }

    private void setInnerTextExtractionError(int resultDelayMs) {
        doAnswer(
                        invocationOnMock -> {
                            Callback<Optional<String>> callback =
                                    (Callback<Optional<String>>)
                                            invocationOnMock.getArgument(1, Callback.class);
                            mFakeTimeTestRule.advanceMillis(resultDelayMs);
                            callback.onResult(Optional.empty());
                            return null;
                        })
                .when(mInnerTextNatives)
                .getInnerText(eq(mRenderFrameHost), any());
    }

    private void assertCursorContainsErrorMessage(Cursor cursor, String errorMessage) {
        assertNotNull(cursor);
        assertEquals(1, cursor.getCount());
        cursor.moveToFirst();
        var successColumnIndex = cursor.getColumnIndex("success");
        var errorMessageColumnIndex = cursor.getColumnIndex("error_message");
        assertNotEquals(-1, successColumnIndex);
        assertNotEquals(-1, errorMessageColumnIndex);

        assertEquals(0, cursor.getInt(successColumnIndex));
        assertEquals(errorMessage, cursor.getString(errorMessageColumnIndex));
    }

    private void assertCursorContainsValues(Cursor cursor, String url, String contents) {
        assertNotNull(cursor);
        assertEquals(1, cursor.getCount());
        cursor.moveToFirst();
        var urlColumnIndex = cursor.getColumnIndex("_id");
        var successColumnIndex = cursor.getColumnIndex("success");
        var contentsColumnIndex = cursor.getColumnIndex("contents");
        assertNotEquals(-1, urlColumnIndex);
        assertNotEquals(-1, successColumnIndex);
        assertNotEquals(-1, contentsColumnIndex);

        assertEquals(url, cursor.getString(urlColumnIndex));
        assertEquals(1, cursor.getInt(successColumnIndex));
        assertEquals(contents, cursor.getString(contentsColumnIndex));
    }
}
