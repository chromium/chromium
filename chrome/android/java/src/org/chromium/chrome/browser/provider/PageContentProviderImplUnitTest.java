// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.database.Cursor;
import android.graphics.Color;
import android.net.Uri;

import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;
import org.junit.Assert;
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
import org.chromium.chrome.browser.provider.PageContentProviderMetrics.Format;
import org.chromium.chrome.browser.provider.PageContentProviderMetrics.PageContentProviderEvent;
import org.chromium.chrome.browser.provider.PageContentProviderMetrics.RequestType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.optimization_guide.content.PageContentProtoProviderBridge;
import org.chromium.components.optimization_guide.content.PageContentProtoProviderBridgeJni;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.AnnotatedPageContent;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.ContentAttributeType;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.ContentAttributes;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.ContentNode;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.TextInfo;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.TextSize;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.TextStyle;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.components.ukm.UkmRecorderJni;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.JUnitTestGURLs;

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
    @Mock private PageContentProtoProviderBridge.Natives mPageContentProtoProviderNatives;
    @Mock private UkmRecorder.Natives mUkmRecorderJniMock;

    private PageContentProvider mProvider;

    @Before
    public void setUp() throws Exception {
        // In production code PageContentProvider must be called from the binder thread, disable
        // thread checks for these tests.
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
        PageContentProviderImpl.clearCachedContent(/* invocationId= */ null);
        UkmRecorderJni.setInstanceForTesting(mUkmRecorderJniMock);
        mProvider = new PageContentProvider();

        InnerTextBridgeJni.setInstanceForTesting(mInnerTextNatives);
        PageContentProtoProviderBridgeJni.setInstanceForTesting(mPageContentProtoProviderNatives);
        when(mWebContents.getMainFrame()).thenReturn(mRenderFrameHost);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL);
        when(mActivityTabProvider.get()).thenReturn(mTab);
    }

    @Test
    public void testInvalidUrl() {
        var eventChecker =
                getWatcherForEvent(
                        RequestType.QUERY,
                        Format.PROTO,
                        PageContentProviderEvent.REQUEST_FAILED_INVALID_URL);
        var result =
                mProvider.query(
                        Uri.parse("content://com.android.invalid/123456"), null, null, null, null);

        assertCursorContainsErrorMessage(result, "Invalid URI");
        eventChecker.assertExpected();
    }

    @Test
    public void testUrlAfterExpiration() throws InterruptedException {
        var structuredDataJson =
                PageContentProviderImpl.getAssistContentStructuredDataForUrl(
                        JUnitTestGURLs.GOOGLE_URL.getSpec(), mActivityTabProvider, false);
        var timeoutEventWatcher =
                getWatcherForEvent(PageContentProviderEvent.URI_INVALIDATED_TIMEOUT);
        var invalidIdEventWatcher =
                getWatcherForEvent(
                        RequestType.QUERY,
                        Format.TEXT,
                        PageContentProviderEvent.REQUEST_FAILED_INVALID_ID,
                        PageContentProviderEvent.REQUEST_STARTED);

        var textContentUri = getMetadataFieldFromJson(structuredDataJson, "content_uri");

        // Run all delayed tasks to ensure the URI is expired.
        ShadowLooper.idleMainLooper(1, TimeUnit.HOURS);

        var resultCursor = mProvider.query(Uri.parse(textContentUri), null, null, null, null);

        assertCursorContainsErrorMessage(resultCursor, "Invalid ID");
        timeoutEventWatcher.assertExpected();
        invalidIdEventWatcher.assertExpected();
    }

    @Test
    public void testGetAssistContentJson() {
        var eventChecker = getWatcherForEvent(PageContentProviderEvent.GET_CONTENT_URI_SUCCESS);
        var structuredDataJson =
                PageContentProviderImpl.getAssistContentStructuredDataForUrl(
                        JUnitTestGURLs.GOOGLE_URL.getSpec(), mActivityTabProvider, false);

        var textContentUri = getMetadataFieldFromJson(structuredDataJson, "content_uri");
        assertNotNull(textContentUri);
        eventChecker.assertExpected();
    }

    @Test
    public void testTextQueryValidContentUrl() {
        setInnerTextExtractionResult("Page contents!", 200);

        var structuredDataJson =
                PageContentProviderImpl.getAssistContentStructuredDataForUrl(
                        JUnitTestGURLs.GOOGLE_URL.getSpec(), mActivityTabProvider, false);
        var eventChecker =
                getWatcherForEvent(
                        RequestType.QUERY,
                        Format.TEXT,
                        PageContentProviderEvent.REQUEST_STARTED,
                        PageContentProviderEvent.REQUEST_SUCCEEDED_RETURNED_EXTRACTED);

        var contentUri = getMetadataFieldFromJson(structuredDataJson, "content_uri");
        // Wait 300ms between creating URI and querying it.
        mFakeTimeTestRule.advanceMillis(300);
        Cursor resultCursor;
        // Time between creating URI and querying it should be recorded. As well as the time spent
        // extracting text and the total time passed.
        try (HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.AssistContent.WebPageContentProvider.Latency.CreateToExtractionStart.Query.Text",
                                300)
                        .expectIntRecord(
                                "Android.AssistContent.WebPageContentProvider.Latency.ExtractionStartToEnd.Query.Text",
                                200)
                        .expectIntRecord(
                                "Android.AssistContent.WebPageContentProvider.Latency.TotalLatency.Query.Text",
                                300 + 200)
                        .build()) {
            resultCursor = mProvider.query(Uri.parse(contentUri), null, null, null, null);
        }
        assertTextCursorContainsValues(
                resultCursor,
                JUnitTestGURLs.GOOGLE_URL.getSpec(),
                /* textContents= */ "Page contents!");
        verify(mInnerTextNatives).getInnerText(eq(mRenderFrameHost), any());
        eventChecker.assertExpected();
    }

    @Test
    public void testTextQuery_errorWhileExtracting() {
        setInnerTextExtractionError(100);
        var structuredDataJson =
                PageContentProviderImpl.getAssistContentStructuredDataForUrl(
                        JUnitTestGURLs.GOOGLE_URL.getSpec(), mActivityTabProvider, false);
        var eventChecker =
                getWatcherForEvent(
                        RequestType.QUERY,
                        Format.TEXT,
                        PageContentProviderEvent.REQUEST_STARTED,
                        PageContentProviderEvent.REQUEST_FAILED_EMPTY_RESULT);

        var contentUri = getMetadataFieldFromJson(structuredDataJson, "content_uri");
        mFakeTimeTestRule.advanceMillis(300);
        Cursor resultCursor = mProvider.query(Uri.parse(contentUri), null, null, null, null);
        assertCursorContainsErrorMessage(resultCursor, "Error during extraction");
        eventChecker.assertExpected();
    }

    @Test
    public void testProtoQueryValidContentUri() {
        var nodeContents = "Page contents!";
        var apcProto = getAnnotatedPageContentsProto(nodeContents);
        setProtoContentExtractionResult(apcProto, 100);
        var eventChecker =
                getWatcherForEvent(
                        RequestType.QUERY,
                        Format.PROTO,
                        PageContentProviderEvent.REQUEST_STARTED,
                        PageContentProviderEvent.REQUEST_SUCCEEDED_RETURNED_EXTRACTED);

        var structuredDataJson =
                PageContentProviderImpl.getAssistContentStructuredDataForUrl(
                        JUnitTestGURLs.GOOGLE_URL.getSpec(), mActivityTabProvider, false);
        var protoContentUri = getMetadataFieldFromJson(structuredDataJson, "proto_content_uri");

        Cursor resultCursor = mProvider.query(Uri.parse(protoContentUri), null, null, null, null);

        assertProtoCursorContainsValues(
                resultCursor, JUnitTestGURLs.GOOGLE_URL.getSpec(), apcProto.toByteArray());
        eventChecker.assertExpected();
    }

    private HistogramWatcher getWatcherForEvent(@PageContentProviderEvent int event) {
        return HistogramWatcher.newSingleRecordWatcher(
                "Android.AssistContent.WebPageContentProvider.Events", event);
    }

    private HistogramWatcher getWatcherForEvent(
            @RequestType int requestType,
            @Format int format,
            @PageContentProviderEvent int... events) {
        var histogramName =
                PageContentProviderMetrics.concatenateTypeAndFormatToHistogramName(
                        "Android.AssistContent.WebPageContentProvider.Events", requestType, format);
        return HistogramWatcher.newBuilder().expectIntRecords(histogramName, events).build();
    }

    private String getMetadataFieldFromJson(String jsonString, String fieldName) {
        try {
            JSONObject jsonObject = (JSONObject) new JSONTokener(jsonString).nextValue();
            return jsonObject.getJSONObject("page_metadata").getString(fieldName);
        } catch (JSONException e) {
            Assert.fail("Error parsing metadata json");
            return null;
        }
    }

    private void setInnerTextExtractionResult(String result, int resultDelayMs) {
        doAnswer(
                        invocationOnMock -> {
                            Callback<String> callback =
                                    (Callback<String>)
                                            invocationOnMock.getArgument(1, Callback.class);
                            mFakeTimeTestRule.advanceMillis(resultDelayMs);
                            callback.onResult(result);
                            return null;
                        })
                .when(mInnerTextNatives)
                .getInnerText(eq(mRenderFrameHost), any());
    }

    private void setProtoContentExtractionResult(AnnotatedPageContent proto, int resultDelayMs) {
        doAnswer(
                        invocationOnMock -> {
                            Callback<byte[]> callback =
                                    (Callback<byte[]>)
                                            invocationOnMock.getArgument(1, Callback.class);
                            mFakeTimeTestRule.advanceMillis(resultDelayMs);
                            callback.onResult(proto.toByteArray());
                            return null;
                        })
                .when(mPageContentProtoProviderNatives)
                .getAiPageContent(eq(mWebContents), any());
    }

    private void setInnerTextExtractionError(int resultDelayMs) {
        doAnswer(
                        invocationOnMock -> {
                            Callback<String> callback =
                                    (Callback<String>)
                                            invocationOnMock.getArgument(1, Callback.class);
                            mFakeTimeTestRule.advanceMillis(resultDelayMs);
                            callback.onResult(null);
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

    private void assertTextCursorContainsValues(Cursor cursor, String url, String textContents) {
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
        assertEquals(textContents, cursor.getString(contentsColumnIndex));
    }

    private void assertProtoCursorContainsValues(Cursor cursor, String url, byte[] protoContents) {
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
        assertArrayEquals(protoContents, cursor.getBlob(contentsColumnIndex));
    }

    private AnnotatedPageContent getAnnotatedPageContentsProto(String nodeText) {
        AnnotatedPageContent.Builder pageContentBuilder = AnnotatedPageContent.newBuilder();
        var rootNode = getRootContentNode(1);
        var textNode =
                getContentNodeWithText(
                        2,
                        nodeText,
                        TextSize.TEXT_SIZE_M_DEFAULT,
                        Color.GREEN,
                        /* hasEmphasis= */ true);

        rootNode.addChildrenNodes(textNode);
        pageContentBuilder.setRootNode(rootNode);

        return pageContentBuilder.build();
    }

    private ContentNode.Builder getContentNodeWithText(
            int nodeId, String text, TextSize size, int color, boolean hasEmphasis) {
        return ContentNode.newBuilder()
                .setContentAttributes(
                        ContentAttributes.newBuilder()
                                .setCommonAncestorDomNodeId(nodeId)
                                .setAttributeType(ContentAttributeType.CONTENT_ATTRIBUTE_TEXT)
                                .setTextData(
                                        TextInfo.newBuilder()
                                                .setTextContent(text)
                                                .setTextStyle(
                                                        TextStyle.newBuilder()
                                                                .setColor(color)
                                                                .setHasEmphasis(hasEmphasis)
                                                                .setTextSize(size))));
    }

    private ContentNode.Builder getRootContentNode(int rootNodeId) {
        return ContentNode.newBuilder()
                .setContentAttributes(
                        ContentAttributes.newBuilder()
                                .setCommonAncestorDomNodeId(rootNodeId)
                                .setAttributeType(ContentAttributeType.CONTENT_ATTRIBUTE_ROOT));
    }
}
