// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Rect;
import android.net.Uri;
import android.view.View;

import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;

import org.json.JSONArray;
import org.json.JSONTokener;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.components.content_capture.ContentCaptureConsumer;
import org.chromium.components.content_capture.ContentCaptureData;
import org.chromium.components.content_capture.ContentCaptureDataBase;
import org.chromium.components.content_capture.ContentCaptureFrame;
import org.chromium.components.content_capture.ContentCaptureTestSupport;
import org.chromium.components.content_capture.FrameSession;
import org.chromium.components.content_capture.OnscreenContentProvider;
import org.chromium.components.content_capture.UrlAllowlist;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Tests for content capture. Those cases could become flaky when renderer is busy, because
 * ContentCapture task is run in best effort priority, we will see if this is real problem for
 * testing.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@CommandLineFlags.Add({"enable-features=ContentCapture"})
public class AwContentCaptureTest extends AwParameterizedTest {
    private static class TestAwContentCaptureConsumer implements ContentCaptureConsumer {
        private static final long DEFAULT_TIMEOUT_IN_SECONDS = 30;

        public static final int CONTENT_CAPTURED = 1;
        public static final int CONTENT_UPDATED = 2;
        public static final int CONTENT_REMOVED = 3;
        public static final int SESSION_REMOVED = 4;
        public static final int TITLE_UPDATED = 5;
        public static final int FAVICON_UPDATED = 6;

        public TestAwContentCaptureConsumer() {
            mCapturedContentIds = new HashSet<Long>();
        }

        public void setAllowURL(String host) {
            HashSet<String> allowedUrls = new HashSet<>();
            allowedUrls.add(host);
            mUrlAllowlist = new UrlAllowlist(allowedUrls, null);
        }

        @Override
        public void onContentCaptured(
                FrameSession parentFrame, ContentCaptureFrame contentCaptureFrame) {
            mParentFrame = parentFrame;
            mCapturedContent = contentCaptureFrame;
            for (ContentCaptureDataBase child : contentCaptureFrame.getChildren()) {
                mCapturedContentIds.add(child.getId());
            }
            mCallbacks.add(CONTENT_CAPTURED);
            mCallbackHelper.notifyCalled();
        }

        @Override
        public void onContentUpdated(
                FrameSession parentFrame, ContentCaptureFrame contentCaptureFrame) {
            mParentFrame = parentFrame;
            mUpdatedContent = contentCaptureFrame;
            mCallbacks.add(CONTENT_UPDATED);
            mCallbackHelper.notifyCalled();
        }

        @Override
        public void onSessionRemoved(FrameSession session) {
            mRemovedSession = session;
            mCallbacks.add(SESSION_REMOVED);
            mCallbackHelper.notifyCalled();
        }

        @Override
        public void onContentRemoved(FrameSession session, long[] removedIds) {
            mCurrentFrameSession = session;
            mRemovedIds = removedIds;
            // Remove the id from removedIds because id can be reused.
            for (long id : removedIds) {
                mCapturedContentIds.remove(id);
            }
            mCallbacks.add(CONTENT_REMOVED);
            mCallbackHelper.notifyCalled();
        }

        @Override
        public void onTitleUpdated(ContentCaptureFrame contentCaptureFrame) {
            mTitleUpdatedFrame = contentCaptureFrame;
            mCallbacks.add(TITLE_UPDATED);
            mCallbackHelper.notifyCalled();
        }

        @Override
        public void onFaviconUpdated(ContentCaptureFrame contentCaptureFrame) {
            mFaviconUpdatedFrame = contentCaptureFrame;
            mCallbacks.add(FAVICON_UPDATED);
            mCallbackHelper.notifyCalled();
        }

        @Override
        public boolean shouldCapture(String[] urls) {
            if (mUrlAllowlist == null) return true;
            return mUrlAllowlist.isAllowed(urls);
        }

        public FrameSession getParentFrame() {
            return mParentFrame;
        }

        public ContentCaptureFrame getCapturedContent() {
            return mCapturedContent;
        }

        public ContentCaptureFrame getUpdatedContent() {
            return mUpdatedContent;
        }

        public ContentCaptureFrame getFaviconUpdatedFrame() {
            return mFaviconUpdatedFrame;
        }

        public FrameSession getCurrentFrameSession() {
            return mCurrentFrameSession;
        }

        public FrameSession getRemovedSession() {
            return mRemovedSession;
        }

        public long[] getRemovedIds() {
            return mRemovedIds;
        }

        public void reset() {
            mParentFrame = null;
            mCapturedContent = null;
            mUpdatedContent = null;
            mCurrentFrameSession = null;
            mRemovedIds = null;
            mCallbacks.clear();
        }

        public void waitForCallback(int currentCallCount) throws Exception {
            waitForCallback(currentCallCount, 1);
        }

        public void waitForCallback(int currentCallCount, int numberOfCallsToWaitFor)
                throws Exception {
            mCallbackHelper.waitForCallback(
                    currentCallCount,
                    numberOfCallsToWaitFor,
                    DEFAULT_TIMEOUT_IN_SECONDS,
                    TimeUnit.SECONDS);
            mCallCount += numberOfCallsToWaitFor;
        }

        public int getCallCount() {
            return mCallCount;
        }

        public Set<Long> cloneCaptureContentIds() {
            return new HashSet<Long>(mCapturedContentIds);
        }

        public int[] getCallbacks() {
            int[] result = new int[mCallbacks.size()];
            int index = 0;
            for (Integer c : mCallbacks) {
                result[index++] = c;
            }
            return result;
        }

        // Use our own call count to avoid unexpected callback issue.
        private int mCallCount;
        // TODO: (crbug.com/1121827) Remove volatile if possible.
        private volatile Set<Long> mCapturedContentIds;
        private volatile FrameSession mParentFrame;
        private volatile ContentCaptureFrame mCapturedContent;
        private volatile ContentCaptureFrame mUpdatedContent;
        private volatile FrameSession mCurrentFrameSession;
        private volatile FrameSession mRemovedSession;
        private volatile long[] mRemovedIds;
        private volatile ContentCaptureFrame mTitleUpdatedFrame;
        private volatile ContentCaptureFrame mFaviconUpdatedFrame;
        private volatile ArrayList<Integer> mCallbacks = new ArrayList<Integer>();

        private CallbackHelper mCallbackHelper = new CallbackHelper();
        private volatile UrlAllowlist mUrlAllowlist;
    }

    private static final String MAIN_FRAME_FILE = "/main_frame.html";
    private static final String SECOND_PAGE = "/second_page.html";

    @Rule public AwActivityTestRule mRule;

    private TestWebServer mWebServer;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private AwTestContainerView mContainerView;
    private TestAwContentCaptureConsumer mConsumer;
    private TestAwContentCaptureConsumer mSecondConsumer;
    private OnscreenContentProvider mOnscreenContentProvider;

    public AwContentCaptureTest(AwSettingsMutation param) {
        this.mRule = new AwActivityTestRule(param.getMutation());
    }

    private void loadUrlSync(String url) {
        try {
            mRule.loadUrlSync(
                    mContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper(), url);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private String executeJavaScriptAndWaitForResult(String code) throws Throwable {
        return mRule.executeJavaScriptAndWaitForResult(
                mContainerView.getAwContents(), mContentsClient, code);
    }

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();
        mContentsClient = new TestAwContentsClient();
        mContainerView = mRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mConsumer = new TestAwContentCaptureConsumer();
                    mOnscreenContentProvider =
                            new OnscreenContentProvider(
                                    mRule.getActivity(),
                                    mContainerView,
                                    mAwContents.getWebContents());
                    mOnscreenContentProvider.addConsumer(mConsumer);
                    mOnscreenContentProvider.removePlatformConsumerForTesting();
                    mAwContents.setOnscreenContentProvider(mOnscreenContentProvider);
                });
    }

    private void insertElement(String id, String content) {
        String script =
                "var place_holder = document.getElementById('place_holder');"
                        + "place_holder.insertAdjacentHTML('beforebegin', '<p id=\\'"
                        + id
                        + "\\'>"
                        + content
                        + "</p>');";
        runScript(script);
    }

    private void setInnerHTML(String id, String content) {
        String script =
                "var el = document.getElementById('"
                        + id
                        + "');"
                        + "el.innerHTML='"
                        + content
                        + "';";
        runScript(script);
    }

    private void removeElement(String id) {
        String script =
                "var el = document.getElementById('"
                        + id
                        + "');"
                        + "document.body.removeChild(el);";
        runScript(script);
    }

    private void runScript(String script) {
        try {
            executeJavaScriptAndWaitForResult(script);
        } catch (Throwable e) {
            throw new RuntimeException(e);
        }
    }

    private void destroyAwContents() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.destroy();
                });
    }

    private void scrollToBottom() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContainerView.scrollTo(0, mContainerView.getHeight());
                });
    }

    private void changeContent(String id, String content) {
        String script =
                "var el = document.getElementById('"
                        + id
                        + "');"
                        + "el.firstChild.textContent = '"
                        + content
                        + "';";
        runScript(script);
    }

    private void scrollToTop() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContainerView.scrollTo(0, 0);
                });
    }

    private static void verifyFrame(
            Long expectedId, String expectedUrl, String title, ContentCaptureFrame result) {
        if (expectedId == null || expectedId.longValue() == 0) {
            Assert.assertNotEquals(0, result.getId());
        } else {
            Assert.assertEquals(expectedId.longValue(), result.getId());
        }
        Assert.assertEquals(title, result.getTitle());
        Assert.assertEquals(title, result.getText());
        Assert.assertEquals(expectedUrl, result.getUrl());
        Assert.assertFalse(result.getBounds().isEmpty());
    }

    private static void verifyFrameSession(FrameSession expected, FrameSession result) {
        if (expected == null && (result == null || result.isEmpty())) return;
        Assert.assertEquals(expected.size(), result.size());
        for (int i = 0; i < expected.size(); i++) {
            verifyFrame(
                    expected.get(i).getId(),
                    expected.get(i).getUrl(),
                    expected.get(i).getTitle(),
                    result.get(i));
        }
    }

    private static void verifyContent(
            Set<String> expectedContent,
            Set<Long> unexpectedIds,
            Set<Long> expectedIds,
            ContentCaptureFrame result) {
        Assert.assertEquals(expectedContent.size(), result.getChildren().size());
        if (expectedIds != null) {
            Assert.assertEquals(expectedIds.size(), result.getChildren().size());
        }
        for (ContentCaptureDataBase child : result.getChildren()) {
            Assert.assertTrue(expectedContent.contains(((ContentCaptureData) child).getValue()));
            expectedContent.remove(((ContentCaptureData) child).getValue());
            if (unexpectedIds != null) {
                Assert.assertFalse(unexpectedIds.contains(child.getId()));
            }
            if (expectedIds != null) {
                Assert.assertTrue(expectedIds.contains(child.getId()));
            }
            Assert.assertFalse(child.getBounds().isEmpty());
        }
        Assert.assertTrue(expectedContent.isEmpty());
    }

    private static void verifyCapturedContent(
            FrameSession expectedParentSession,
            Long expectedFrameId,
            String expectedUrl,
            String expectedTitle,
            Set<String> expectedContent,
            Set<Long> unexpectedContentIds,
            FrameSession parentResult,
            ContentCaptureFrame result) {
        verifyFrameSession(expectedParentSession, parentResult);
        // Title is only set to main frame.
        if (expectedParentSession == null || expectedParentSession.isEmpty()) {
            verifyFrame(expectedFrameId, expectedUrl, expectedTitle, result);
        } else {
            verifyFrame(expectedFrameId, expectedUrl, null, result);
        }

        verifyContent(expectedContent, unexpectedContentIds, null, result);
    }

    private static void verifyUpdatedContent(
            FrameSession expectedParentSession,
            Long expectedFrameId,
            String expectedUrl,
            Set<String> expectedContent,
            Set<Long> expectedContentIds,
            FrameSession parentResult,
            ContentCaptureFrame result) {
        verifyFrameSession(expectedParentSession, parentResult);
        verifyFrame(expectedFrameId, expectedUrl, null, result);
        verifyContent(expectedContent, null, expectedContentIds, result);
    }

    private static void verifyRemovedIds(Set<Long> expectedIds, long[] result) {
        Assert.assertEquals(expectedIds.size(), result.length);
        Set<Long> resultSet = new HashSet<Long>(result.length);
        for (long id : result) {
            resultSet.add(id);
        }
        Assert.assertTrue(expectedIds.containsAll(resultSet));
    }

    private static void verifyRemovedContent(
            Long expectedFrameId,
            String expectedUrl,
            Set<Long> expectedIds,
            FrameSession resultFrame,
            long[] result) {
        Assert.assertEquals(1, resultFrame.size());
        verifyFrame(expectedFrameId, expectedUrl, null, resultFrame.get(0));
        verifyRemovedIds(expectedIds, result);
    }

    private static void verifyCallbacks(int[] expectedCallbacks, int[] results) {
        Assert.assertArrayEquals(
                "Expect: "
                        + Arrays.toString(expectedCallbacks)
                        + " Result: "
                        + Arrays.toString(results),
                expectedCallbacks,
                results);
    }

    private static void waitAndVerifyCallbacks(
            int[] expectedCallbacks, int callCount, TestAwContentCaptureConsumer consumer)
            throws Throwable {
        try {
            consumer.waitForCallback(callCount, expectedCallbacks.length);
        } finally {
            verifyCallbacks(expectedCallbacks, consumer.getCallbacks());
        }
    }

    private void runAndVerifyCallbacks(final Runnable testCase, int[] expectedCallbacks)
            throws Throwable {
        try {
            int callCount = mConsumer.getCallCount();
            mConsumer.reset();
            testCase.run();
            mConsumer.waitForCallback(callCount, expectedCallbacks.length);
        } finally {
            verifyCallbacks(expectedCallbacks, mConsumer.getCallbacks());
        }
    }

    private FrameSession createFrameSession(ContentCaptureFrame data) {
        FrameSession session = new FrameSession(1);
        ContentCaptureFrame c = data;
        Rect r = c.getBounds();
        session.add(
                ContentCaptureFrame.createContentCaptureFrame(
                        c.getId(), c.getUrl(), r.left, r.top, r.width(), r.height(), null, null));
        return session;
    }

    private FrameSession createFrameSession(String url) {
        FrameSession session = new FrameSession(1);
        session.add(ContentCaptureFrame.createContentCaptureFrame(0, url, 0, 0, 0, 0, null, null));
        return session;
    }

    private FrameSession createFrameSession(ContentCaptureFrame... frames) {
        FrameSession result = new FrameSession(frames.length);
        for (ContentCaptureFrame f : frames) {
            result.addAll(createFrameSession(f));
        }
        return result;
    }

    @After
    public void tearDown() {
        mWebServer.shutdown();
    }

    private static Set<String> toStringSet(String... strings) {
        Set<String> result = new HashSet<String>();
        for (String s : strings) {
            result.add(s);
        }
        return result;
    }

    private static Set<Long> toLongSet(Long... longs) {
        Set<Long> result = new HashSet<Long>();
        for (Long s : longs) {
            result.add(s);
        }
        return result;
    }

    private static int[] toIntArray(int... callbacks) {
        return callbacks;
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=ContentCaptureConstantStreaming"})
    public void testSingleFrameWithoutConstantStreaming() throws Throwable {
        final String response =
                "<html><head></head><body>"
                        + "<div id='place_holder'>"
                        + "<p style=\"height: 100vh\">Hello</p>"
                        + "<p>world</p>"
                        + "</body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);
        runAndVerifyCallbacks(
                () -> {
                    loadUrlSync(url);
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        Long frameId = null;
        Set<Long> capturedContentIds = null;
        // Verify only on-screen content is captured.
        verifyCapturedContent(
                null,
                frameId,
                url,
                null,
                toStringSet("Hello"),
                capturedContentIds,
                mConsumer.getParentFrame(),
                mConsumer.getCapturedContent());

        frameId = Long.valueOf(mConsumer.getCapturedContent().getId());
        capturedContentIds = mConsumer.cloneCaptureContentIds();
        runAndVerifyCallbacks(
                () -> {
                    scrollToBottom();
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        verifyCapturedContent(
                null,
                frameId,
                url,
                null,
                toStringSet("world"),
                capturedContentIds,
                mConsumer.getParentFrame(),
                mConsumer.getCapturedContent());

        final String newContentId = "new_content_id";
        final String newContent = "new content";
        // Only new content is captured, the content that has been captured will not be captured
        // again.
        runAndVerifyCallbacks(
                () -> {
                    insertElement(newContentId, newContent);
                    scrollToTop();
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        verifyCapturedContent(
                null,
                frameId,
                url,
                null,
                toStringSet(newContent),
                capturedContentIds,
                mConsumer.getParentFrame(),
                mConsumer.getCapturedContent());

        // Changed previous added element, this will trigger remove/capture events.
        long removedContentId = mConsumer.getCapturedContent().getChildren().get(0).getId();
        final String newContent2 = "new content 2";
        capturedContentIds = mConsumer.cloneCaptureContentIds();
        runAndVerifyCallbacks(
                () -> {
                    setInnerHTML(newContentId, newContent2);
                },
                toIntArray(
                        TestAwContentCaptureConsumer.CONTENT_REMOVED,
                        TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        verifyRemovedContent(
                frameId,
                url,
                toLongSet(removedContentId),
                mConsumer.getCurrentFrameSession(),
                mConsumer.getRemovedIds());
        verifyCapturedContent(
                null,
                frameId,
                url,
                null,
                toStringSet(newContent2),
                capturedContentIds,
                mConsumer.getParentFrame(),
                mConsumer.getCapturedContent());

        // Remove the element.
        removedContentId = mConsumer.getCapturedContent().getChildren().get(0).getId();
        capturedContentIds = mConsumer.cloneCaptureContentIds();
        runAndVerifyCallbacks(
                () -> {
                    removeElement(newContentId);
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_REMOVED));
        verifyRemovedContent(
                frameId,
                url,
                toLongSet(removedContentId),
                mConsumer.getCurrentFrameSession(),
                mConsumer.getRemovedIds());
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=ContentCaptureConstantStreaming"})
    public void testSingleFrameWithConstantStreaming() throws Throwable {
        final String response =
                "<html><head></head><body>"
                        + "<div id='place_holder'>"
                        + "<p style=\"height: 100vh\">Hello</p>"
                        + "<p>world</p>"
                        + "</body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);
        runAndVerifyCallbacks(
                () -> {
                    loadUrlSync(url);
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        Long frameId = null;
        Set<Long> capturedContentIds = null;
        // Verify only on-screen content is captured.
        verifyCapturedContent(
                null,
                frameId,
                url,
                null,
                toStringSet("Hello"),
                capturedContentIds,
                mConsumer.getParentFrame(),
                mConsumer.getCapturedContent());

        // Scrolls to the bottom, the node that became invisible is removed, and the content
        // at bottom is captured.
        frameId = Long.valueOf(mConsumer.getCapturedContent().getId());
        long contentHelloId = mConsumer.getCapturedContent().getChildren().get(0).getId();
        capturedContentIds = mConsumer.cloneCaptureContentIds();
        runAndVerifyCallbacks(
                () -> {
                    scrollToBottom();
                },
                toIntArray(
                        TestAwContentCaptureConsumer.CONTENT_CAPTURED,
                        TestAwContentCaptureConsumer.CONTENT_REMOVED));
        verifyCapturedContent(
                null,
                frameId,
                url,
                null,
                toStringSet("world"),
                capturedContentIds,
                mConsumer.getParentFrame(),
                mConsumer.getCapturedContent());
        verifyRemovedContent(
                frameId,
                url,
                toLongSet(contentHelloId),
                mConsumer.getCurrentFrameSession(),
                mConsumer.getRemovedIds());
        long contentWorldId = mConsumer.getCapturedContent().getChildren().get(0).getId();
        // Adds the new content at the beginning and scroll back, the newly visible content
        // is captured and invisible content is removed.
        final String newContentId = "new_content_id";
        final String newContent = "new content";
        capturedContentIds = mConsumer.cloneCaptureContentIds();
        runAndVerifyCallbacks(
                () -> {
                    insertElement(newContentId, newContent);
                    scrollToTop();
                },
                toIntArray(
                        TestAwContentCaptureConsumer.CONTENT_CAPTURED,
                        TestAwContentCaptureConsumer.CONTENT_REMOVED));
        verifyCapturedContent(
                null,
                frameId,
                url,
                null,
                toStringSet(newContent, "Hello"),
                capturedContentIds,
                mConsumer.getParentFrame(),
                mConsumer.getCapturedContent());
        verifyRemovedContent(
                frameId,
                url,
                toLongSet(contentWorldId),
                mConsumer.getCurrentFrameSession(),
                mConsumer.getRemovedIds());

        // Changed previous added element, this will trigger remove/capture events.
        long removedContentId = mConsumer.getCapturedContent().getChildren().get(0).getId();
        // The id is unordered, if the current one is "Hello", the next child must be "new content".
        if (removedContentId == contentHelloId) {
            removedContentId = mConsumer.getCapturedContent().getChildren().get(1).getId();
        }
        final String newContent2 = "new content 2";
        capturedContentIds = mConsumer.cloneCaptureContentIds();
        runAndVerifyCallbacks(
                () -> {
                    setInnerHTML(newContentId, newContent2);
                },
                toIntArray(
                        TestAwContentCaptureConsumer.CONTENT_REMOVED,
                        TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        verifyRemovedContent(
                frameId,
                url,
                toLongSet(removedContentId),
                mConsumer.getCurrentFrameSession(),
                mConsumer.getRemovedIds());
        verifyCapturedContent(
                null,
                frameId,
                url,
                null,
                toStringSet(newContent2),
                capturedContentIds,
                mConsumer.getParentFrame(),
                mConsumer.getCapturedContent());

        // Remove the element.
        removedContentId = mConsumer.getCapturedContent().getChildren().get(0).getId();
        capturedContentIds = mConsumer.cloneCaptureContentIds();
        runAndVerifyCallbacks(
                () -> {
                    removeElement(newContentId);
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_REMOVED));
        verifyRemovedContent(
                frameId,
                url,
                toLongSet(removedContentId),
                mConsumer.getCurrentFrameSession(),
                mConsumer.getRemovedIds());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testChangeContent() throws Throwable {
        final String response =
                "<html><head></head><body>"
                        + "<div id='editable_id'>Hello</div>"
                        + "</div></body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);
        runAndVerifyCallbacks(
                () -> {
                    loadUrlSync(url);
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        Long frameId = null;
        Set<Long> capturedContentIds = null;
        // Verify only on-screen content is captured.
        verifyCapturedContent(
                null,
                frameId,
                url,
                null,
                toStringSet("Hello"),
                capturedContentIds,
                mConsumer.getParentFrame(),
                mConsumer.getCapturedContent());

        // Change the content, we shall get content updated callback.
        frameId = Long.valueOf(mConsumer.getCapturedContent().getId());
        capturedContentIds = mConsumer.cloneCaptureContentIds();
        final String changeContent = "Hello world";
        runAndVerifyCallbacks(
                () -> {
                    changeContent("editable_id", changeContent);
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_UPDATED));
        verifyUpdatedContent(
                null,
                frameId,
                url,
                toStringSet(changeContent),
                capturedContentIds,
                mConsumer.getParentFrame(),
                mConsumer.getUpdatedContent());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRemoveSession() throws Throwable {
        final String response =
                "<html><head></head><body>"
                        + "<div id='editable_id'>Hello</div>"
                        + "</div></body></html>";
        final String response2 =
                "<html><head></head><body>"
                        + "<div id='editable_id'>World</div>"
                        + "</div></body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);
        final String url2 = mWebServer.setResponse(SECOND_PAGE, response2, null);

        runAndVerifyCallbacks(
                () -> {
                    loadUrlSync(url);
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        Long frameId = null;
        Set<Long> capturedContentIds = null;
        verifyCapturedContent(
                null,
                frameId,
                url,
                null,
                toStringSet("Hello"),
                capturedContentIds,
                mConsumer.getParentFrame(),
                mConsumer.getCapturedContent());

        // Keep a copy of current session to verify it removed later.
        FrameSession removedSession = createFrameSession(mConsumer.getCapturedContent());
        capturedContentIds = mConsumer.cloneCaptureContentIds();
        int[] expectedCallbacks =
                toIntArray(
                        TestAwContentCaptureConsumer.SESSION_REMOVED,
                        TestAwContentCaptureConsumer.CONTENT_CAPTURED);
        runAndVerifyCallbacks(
                () -> {
                    loadUrlSync(url2);
                },
                expectedCallbacks);
        verifyCapturedContent(
                null,
                frameId,
                url2,
                null,
                toStringSet("World"),
                capturedContentIds,
                mConsumer.getParentFrame(),
                mConsumer.getCapturedContent());
        // Verify previous session has been removed.
        verifyFrameSession(removedSession, mConsumer.getRemovedSession());

        // Keep a copy of current session to verify it removed later.
        removedSession = createFrameSession(mConsumer.getCapturedContent());
        runAndVerifyCallbacks(
                () -> {
                    destroyAwContents();
                },
                toIntArray(TestAwContentCaptureConsumer.SESSION_REMOVED));
        verifyFrameSession(removedSession, mConsumer.getRemovedSession());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRemoveIframe() throws Throwable {
        final String subFrame =
                "<html><head></head><body>"
                        + "<div id='editable_id'>Hello</div>"
                        + "</div></body></html>";
        final String subFrameUrl = mWebServer.setResponse(SECOND_PAGE, subFrame, null);
        final String mainFrame =
                "<html><head></head><body>"
                        + "<iframe id='sub_frame_id' src='"
                        + subFrameUrl
                        + "'></iframe></body></html>";
        final String mainFrameUrl = mWebServer.setResponse(MAIN_FRAME_FILE, mainFrame, null);
        runAndVerifyCallbacks(
                () -> {
                    loadUrlSync(mainFrameUrl);
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));

        FrameSession expectedParentFrameSession = createFrameSession(mainFrameUrl);
        Long frameId = null;
        verifyCapturedContent(
                expectedParentFrameSession,
                frameId,
                subFrameUrl,
                null,
                toStringSet("Hello"),
                null,
                mConsumer.getParentFrame(),
                mConsumer.getCapturedContent());

        FrameSession removedSession =
                createFrameSession(
                        mConsumer.getCapturedContent(), mConsumer.getParentFrame().get(0));
        runAndVerifyCallbacks(
                () -> {
                    runScript(
                            "var frame = document.getElementById('sub_frame_id');"
                                    + "frame.parentNode.removeChild(frame);");
                },
                toIntArray(TestAwContentCaptureConsumer.SESSION_REMOVED));
        verifyFrameSession(removedSession, mConsumer.getRemovedSession());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMultipleConsumers() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSecondConsumer = new TestAwContentCaptureConsumer();
                    mOnscreenContentProvider.addConsumer(mSecondConsumer);
                });
        int callCount = mSecondConsumer.getCallCount();
        final String response =
                "<html><head></head><body>"
                        + "<div id='place_holder'>"
                        + "<p style=\"height: 100vh\">Hello</p>"
                        + "<p>world</p>"
                        + "</body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);
        runAndVerifyCallbacks(
                () -> {
                    loadUrlSync(url);
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        // Verify the other one also get the content.
        waitAndVerifyCallbacks(
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED),
                callCount,
                mSecondConsumer);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=ContentCaptureTriggeringForExperiment"})
    public void testHostNotAllowed() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSecondConsumer = new TestAwContentCaptureConsumer();
                });
        final String response =
                "<html><head></head><body>"
                        + "<div id='place_holder'>"
                        + "<p style=\"height: 100vh\">Hello</p>"
                        + "<p>world</p>"
                        + "</body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);
        mSecondConsumer.setAllowURL("www.chromium.org");
        runAndVerifyCallbacks(
                () -> {
                    loadUrlSync(url);
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        // Verify the other one didn't get the content.
        Assert.assertEquals(0, mSecondConsumer.getCallbacks().length);
    }

    private void runHostAllowedTest() throws Throwable {
        final String response =
                "<html><head></head><body>"
                        + "<div id='place_holder'>"
                        + "<p style=\"height: 100vh\">Hello</p>"
                        + "<p>world</p>"
                        + "</body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);
        mConsumer.setAllowURL(Uri.parse(url).getHost());
        runAndVerifyCallbacks(
                () -> {
                    loadUrlSync(url);
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=ContentCaptureTriggeringForExperiment"})
    public void testHostAllowed() throws Throwable {
        runHostAllowedTest();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=ContentCaptureTriggeringForExperiment"})
    public void testHostAllowedForExperiment() throws Throwable {
        runHostAllowedTest();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=ContentCaptureTriggeringForExperiment"})
    public void testCantCreateExperimentConsumer() throws Throwable {
        List<ContentCaptureConsumer> consumers = mOnscreenContentProvider.getConsumersForTesting();
        Assert.assertEquals(1, consumers.size());
        Assert.assertTrue(consumers.get(0) instanceof TestAwContentCaptureConsumer);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHideAndShow() throws Throwable {
        final String response =
                "<html><head></head><body>"
                        + "<div id='editable_id'>Hello</div>"
                        + "</div></body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);
        runAndVerifyCallbacks(
                () -> {
                    loadUrlSync(url);
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));

        // Hides and shows the WebContent and verifies the content is captured again.
        runAndVerifyCallbacks(
                () -> {
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> {
                                mContainerView.onWindowVisibilityChanged(View.INVISIBLE);
                            });
                    AwActivityTestRule.pollInstrumentationThread(
                            () -> !mAwContents.isPageVisible());
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> {
                                mContainerView.onWindowVisibilityChanged(View.VISIBLE);
                            });
                    AwActivityTestRule.pollInstrumentationThread(() -> mAwContents.isPageVisible());
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testTitle() throws Throwable {
        final String response =
                "<html><head><title>Hello</title></head><body>" + "<p>world</p>" + "</body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);
        runAndVerifyCallbacks(
                () -> {
                    loadUrlSync(url);
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        Long frameId = null;
        Set<Long> capturedContentIds = null;
        // Verify only on-screen content is captured.
        verifyCapturedContent(
                null,
                frameId,
                url,
                "Hello",
                toStringSet("world"),
                capturedContentIds,
                mConsumer.getParentFrame(),
                mConsumer.getCapturedContent());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUpdateTitle() throws Throwable {
        final String response =
                "<html><head><title>Hello</title></head><body>" + "<p>world</p>" + "</body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);
        runAndVerifyCallbacks(
                () -> {
                    loadUrlSync(url);
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        Long frameId = null;
        Set<Long> capturedContentIds = null;
        // Verify only on-screen content is captured.
        verifyCapturedContent(
                null,
                frameId,
                url,
                "Hello",
                toStringSet("world"),
                capturedContentIds,
                mConsumer.getParentFrame(),
                mConsumer.getCapturedContent());

        // Update the title and verify the result.
        runAndVerifyCallbacks(
                () -> {
                    runScript("document.title='hello world'");
                },
                toIntArray(TestAwContentCaptureConsumer.TITLE_UPDATED));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFaviconRetrievedAtFirstContentCapture() throws Throwable {
        // Starts with a empty document, so no content shall be streamed.
        final String response =
                "<html><head>"
                        + "<link rel=\"apple-touch-icon\" href=\"image.png\">"
                        + "</head><body>"
                        + "<p id='place_holder'></p>"
                        + "</body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);
        int count = mContentsClient.getTouchIconHelper().getCallCount();
        loadUrlSync(url);
        // To simulate favicon being retrieved by WebContents before first Content is streamed,
        // wait favicon being available in WebContents, then insert the text to document.
        mContentsClient.getTouchIconHelper().waitForCallback(count);
        Assert.assertEquals(1, mContentsClient.getTouchIconHelper().getTouchIconsCount());
        runAndVerifyCallbacks(
                () -> {
                    runScript("document.getElementById('place_holder').innerHTML = 'world';");
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        GURL gurl = new GURL(url);
        String origin = gurl.getOrigin().getSpec();
        // Blink attaches the default favicon if it is not specified in page.
        final String expectedJson =
                String.format(
                        "["
                                + "    {"
                                + "        \"type\" : \"favicon\","
                                + "        \"url\" : \"%sfavicon.ico\""
                                + "    },"
                                + "    {"
                                + "        \"type\" : \"touch icon\","
                                + "        \"url\" : \"%simage.png\""
                                + "    }"
                                + "]",
                        origin, origin);
        verifyFaviconResult(expectedJson, mConsumer.getCapturedContent().getFavicon());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFaviconRetrievedAfterFirstContentCapture() throws Throwable {
        final String response =
                "<html><head'>"
                        + "</head><body>"
                        + "<p id='place_holder'>world</p>"
                        + "</body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);
        // Direct ContentCaptureReveiver and OnscreenContentProvider not to get the favicon
        // from Webontents, because there is no way to control the time of favicon update.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ContentCaptureTestSupport.disableGetFaviconFromWebContents();
                });
        runAndVerifyCallbacks(
                () -> {
                    loadUrlSync(url);
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        GURL gurl = new GURL(url);
        String origin = gurl.getOrigin().getSpec();
        final String expectedJson =
                String.format(
                        "["
                                + "    {"
                                + "        \"type\" : \"favicon\","
                                + "        \"url\" : \"%sfavicon.ico\""
                                + "    },"
                                + "    {"
                                + "        \"type\" : \"touch icon\","
                                + "        \"url\" : \"%simage.png\""
                                + "    }"
                                + "]",
                        origin, origin);
        // Simulates favicon update by calling OnscreenContentProvider's test method.
        runAndVerifyCallbacks(
                () -> {
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> {
                                ContentCaptureTestSupport.simulateDidUpdateFaviconURL(
                                        mAwContents.getWebContents(), expectedJson);
                            });
                },
                toIntArray(TestAwContentCaptureConsumer.FAVICON_UPDATED));
        verifyFaviconResult(expectedJson, mConsumer.getFaviconUpdatedFrame().getFavicon());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFavicon() throws Throwable {
        final String response =
                "<html><head><link rel=icon href=mac.icns sizes=\"128x128 512x512 8192x8192"
                        + " 32768x32768\"></head><body><p>world</p></body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);

        runAndVerifyCallbacks(
                () -> {
                    loadUrlSync(url);
                },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        Long frameId = null;
        Set<Long> capturedContentIds = null;
        // Verify only on-screen content is captured.
        verifyCapturedContent(
                null,
                frameId,
                url,
                null,
                toStringSet("world"),
                capturedContentIds,
                mConsumer.getParentFrame(),
                mConsumer.getCapturedContent());
        // The favicon could be from either first capture or FaviconUpdated callback.
        String favicon = mConsumer.getCapturedContent().getFavicon();
        if (favicon == null) {
            // Update the title and verify the result.
            runAndVerifyCallbacks(
                    () -> {}, toIntArray(TestAwContentCaptureConsumer.FAVICON_UPDATED));
            favicon = mConsumer.getFaviconUpdatedFrame().getFavicon();
        }
        GURL gurl = new GURL(url);
        String origin = gurl.getOrigin().getSpec();
        final String expectedJson =
                String.format(
                        "["
                                + "     {"
                                + "         \"sizes\" : "
                                + "         ["
                                + "             {"
                                + "                 \"height\" : 128,"
                                + "                 \"width\" : 128"
                                + "             },"
                                + "             {"
                                + "                 \"height\" : 512,"
                                + "                 \"width\" : 512"
                                + "             },"
                                + "             {"
                                + "                 \"height\" : 8192,"
                                + "                 \"width\" : 8192"
                                + "             },"
                                + "             {"
                                + "                 \"height\" : 32768,"
                                + "                 \"width\" : 32768"
                                + "             }"
                                + "         ],"
                                + "         \"type\" : \"favicon\","
                                + "         \"url\" : \"%smac.icns\""
                                + "     }"
                                + " ]",
                        origin);
        verifyFaviconResult(expectedJson, favicon);
    }

    private static void verifyFaviconResult(String expectedJson, String resultJson)
            throws Throwable {
        JSONArray expectedResult = (JSONArray) new JSONTokener(expectedJson).nextValue();
        JSONArray actualResult = (JSONArray) new JSONTokener(resultJson).nextValue();
        Assert.assertEquals(
                String.format("Actual:%s\n Expected:\n%s\n", resultJson, expectedJson),
                expectedResult.toString(),
                actualResult.toString());
    }
}
