// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Rect;
import android.support.test.filters.LargeTest;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.components.content_capture.ContentCaptureConsumer;
import org.chromium.components.content_capture.ContentCaptureController;
import org.chromium.components.content_capture.ContentCaptureData;
import org.chromium.components.content_capture.FrameSession;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Tests for content capture. Those cases could become flaky when renderer is busy, because
 * ContentCapture task is run in best effort priority, we will see if this is real problem for
 * testing.
 */
@RunWith(AwJUnit4ClassRunner.class)
@CommandLineFlags.Add({"enable-features=ContentCapture"})
public class AwContentCaptureTest {
    private static class TestAwContentCatpureController extends ContentCaptureController {
        public TestAwContentCatpureController() {
            sContentCaptureController = this;
        }

        @Override
        public boolean shouldStartCapture() {
            return false;
        }

        @Override
        protected void pullWhitelist() {
            String[] whitelist = null;
            boolean[] isRegEx = null;
            if (mWhiteList != null && mIsRegEx != null) {
                mWhiteList.toArray(whitelist);
                isRegEx = new boolean[mWhiteList.size()];
                int i = 0;
                for (boolean r : mIsRegEx) {
                    isRegEx[i++] = r;
                }
            }
            setWhitelist(whitelist, isRegEx);
        }

        private ArrayList<String> mWhiteList;
        private ArrayList<Boolean> mIsRegEx;
    }

    private static class TestAwContentCaptureConsumer extends ContentCaptureConsumer {
        private static final long DEFAULT_TIMEOUT_IN_SECONDS = 30;

        public static final int CONTENT_CAPTURED = 1;
        public static final int CONTENT_UPDATED = 2;
        public static final int CONTENT_REMOVED = 3;
        public static final int SESSION_REMOVED = 4;

        public TestAwContentCaptureConsumer(WebContents webContents) {
            super(webContents);
            mCapturedContentIds = new HashSet<Long>();
        }

        @Override
        public void onContentCaptured(
                FrameSession parentFrame, ContentCaptureData contentCaptureData) {
            mParentFrame = parentFrame;
            mCapturedContent = contentCaptureData;
            for (ContentCaptureData child : contentCaptureData.getChildren()) {
                mCapturedContentIds.add(child.getId());
            }
            mCallbacks.add(CONTENT_CAPTURED);
            mCallbackHelper.notifyCalled();
        }

        @Override
        public void onContentUpdated(
                FrameSession parentFrame, ContentCaptureData contentCaptureData) {
            mParentFrame = parentFrame;
            mUpdatedContent = contentCaptureData;
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

        public FrameSession getParentFrame() {
            return mParentFrame;
        }

        public ContentCaptureData getCapturedContent() {
            return mCapturedContent;
        }

        public ContentCaptureData getUpdatedContent() {
            return mUpdatedContent;
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
            mCallbackHelper.waitForCallback(currentCallCount, numberOfCallsToWaitFor,
                    DEFAULT_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS);
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
        private volatile Set<Long> mCapturedContentIds;
        private volatile FrameSession mParentFrame;
        private volatile ContentCaptureData mCapturedContent;
        private volatile ContentCaptureData mUpdatedContent;
        private volatile FrameSession mCurrentFrameSession;
        private volatile FrameSession mRemovedSession;
        private volatile long[] mRemovedIds;
        private volatile ArrayList<Integer> mCallbacks = new ArrayList<Integer>();

        private CallbackHelper mCallbackHelper = new CallbackHelper();
    }

    private static final String MAIN_FRAME_FILE = "/main_frame.html";
    private static final String SECOND_PAGE = "/second_page.html";

    @Rule
    public AwActivityTestRule mRule = new AwActivityTestRule();

    private TestWebServer mWebServer;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private AwTestContainerView mContainerView;
    private TestAwContentCaptureConsumer mConsumer;
    private TestAwContentCatpureController mController;

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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mController = new TestAwContentCatpureController();
            mConsumer = new TestAwContentCaptureConsumer(mAwContents.getWebContents());
            mAwContents.setContentCaptureConsumer(mConsumer);
        });
    }

    private void insertElement(String id, String content) {
        String script = "var place_holder = document.getElementById('place_holder');"
                + "place_holder.insertAdjacentHTML('beforebegin', '<p id=\\'" + id + "\\'>"
                + content + "</p>');";
        runScript(script);
    }

    private void setInnerHTML(String id, String content) {
        String script = "var el = document.getElementById('" + id + "');"
                + "el.innerHTML='" + content + "';";
        runScript(script);
    }

    private void removeElement(String id) {
        String script = "var el = document.getElementById('" + id + "');"
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
        TestThreadUtils.runOnUiThreadBlocking(() -> { mAwContents.destroy(); });
    }

    private void scrollToBottom() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mContainerView.scrollTo(0, mContainerView.getHeight()); });
    }

    private void changeContent(String id, String content) {
        String script = "var el = document.getElementById('" + id + "');"
                + "el.firstChild.textContent = '" + content + "';";
        runScript(script);
    }

    private void scrollToTop() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mContainerView.scrollTo(0, 0); });
    }

    private static void verifyFrame(
            Long expectedId, String expectedUrl, ContentCaptureData result) {
        if (expectedId == null || expectedId.longValue() == 0) {
            Assert.assertNotEquals(0, result.getId());
        } else {
            Assert.assertEquals(expectedId.longValue(), result.getId());
        }
        Assert.assertEquals(expectedUrl, result.getValue());
        Assert.assertFalse(result.getBounds().isEmpty());
    }

    private static void verifyFrameSession(FrameSession expected, FrameSession result) {
        if (expected == null && (result == null || result.isEmpty())) return;
        Assert.assertEquals(expected.size(), result.size());
        for (int i = 0; i < expected.size(); i++) {
            verifyFrame(expected.get(i).getId(), expected.get(i).getValue(), result.get(i));
        }
    }

    private static void verifyContent(Set<String> expectedContent, Set<Long> unexpectedIds,
            Set<Long> expectedIds, ContentCaptureData result) {
        Assert.assertEquals(expectedContent.size(), result.getChildren().size());
        if (expectedIds != null) {
            Assert.assertEquals(expectedIds.size(), result.getChildren().size());
        }
        for (ContentCaptureData child : result.getChildren()) {
            Assert.assertTrue(expectedContent.contains(child.getValue()));
            expectedContent.remove(child.getValue());
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

    private static void verifyCapturedContent(FrameSession expectedParentSession,
            Long expectedFrameId, String expectedUrl, Set<String> expectedContent,
            Set<Long> unexpectedContentIds, FrameSession parentResult, ContentCaptureData result) {
        verifyFrameSession(expectedParentSession, parentResult);
        verifyFrame(expectedFrameId, expectedUrl, result);
        verifyContent(expectedContent, unexpectedContentIds, null, result);
    }

    private static void verifyUpdatedContent(FrameSession expectedParentSession,
            Long expectedFrameId, String expectedUrl, Set<String> expectedContent,
            Set<Long> expectedContentIds, FrameSession parentResult, ContentCaptureData result) {
        verifyFrameSession(expectedParentSession, parentResult);
        verifyFrame(expectedFrameId, expectedUrl, result);
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

    private static void verifyRemovedContent(Long expectedFrameId, String expectedUrl,
            Set<Long> expectedIds, FrameSession resultFrame, long[] result) {
        Assert.assertEquals(1, resultFrame.size());
        verifyFrame(expectedFrameId, expectedUrl, resultFrame.get(0));
        verifyRemovedIds(expectedIds, result);
    }

    private static void verifyCallbacks(int[] expectedCallbacks, int[] results) {
        Assert.assertArrayEquals("Expect: " + Arrays.toString(expectedCallbacks)
                        + " Result: " + Arrays.toString(results),
                expectedCallbacks, results);
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

    private FrameSession createFrameSession(ContentCaptureData data) {
        FrameSession session = new FrameSession(1);
        ContentCaptureData c = data;
        Rect r = c.getBounds();
        session.add(ContentCaptureData.createContentCaptureData(
                null, c.getId(), c.getValue(), r.left, r.top, r.width(), r.height()));
        return session;
    }

    private FrameSession createFrameSession(String url) {
        FrameSession session = new FrameSession(1);
        session.add(ContentCaptureData.createContentCaptureData(null, 0, url, 0, 0, 0, 0));
        return session;
    }

    private FrameSession createFrameSession(ContentCaptureData... frames) {
        FrameSession result = new FrameSession(frames.length);
        for (ContentCaptureData f : frames) {
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
    public void testSingleFrame() throws Throwable {
        final String response = "<html><head></head><body>"
                + "<div id='place_holder'>"
                + "<p style=\"height: 100vh\">Hello</p>"
                + "<p>world</p>"
                + "</body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);
        runAndVerifyCallbacks(() -> {
            loadUrlSync(url);
        }, toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        Long frameId = null;
        Set<Long> capturedContentIds = null;
        // Verify only on-screen content is captured.
        verifyCapturedContent(null, frameId, url, toStringSet("Hello"), capturedContentIds,
                mConsumer.getParentFrame(), mConsumer.getCapturedContent());

        frameId = Long.valueOf(mConsumer.getCapturedContent().getId());
        capturedContentIds = mConsumer.cloneCaptureContentIds();
        runAndVerifyCallbacks(() -> {
            scrollToBottom();
        }, toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        verifyCapturedContent(null, frameId, url, toStringSet("world"), capturedContentIds,
                mConsumer.getParentFrame(), mConsumer.getCapturedContent());

        final String newContentId = "new_content_id";
        final String newContent = "new content";
        // Only new content is captured, the content that has been captured will not be captured
        // again.
        runAndVerifyCallbacks(() -> {
            insertElement(newContentId, newContent);
            scrollToTop();
        }, toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        verifyCapturedContent(null, frameId, url, toStringSet(newContent), capturedContentIds,
                mConsumer.getParentFrame(), mConsumer.getCapturedContent());

        // Changed previous added element, this will trigger remove/capture events.
        long removedContentId = mConsumer.getCapturedContent().getChildren().get(0).getId();
        final String newContent2 = "new content 2";
        capturedContentIds = mConsumer.cloneCaptureContentIds();
        runAndVerifyCallbacks(()
                                      -> { setInnerHTML(newContentId, newContent2); },
                toIntArray(TestAwContentCaptureConsumer.CONTENT_REMOVED,
                        TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        verifyRemovedContent(frameId, url, toLongSet(removedContentId),
                mConsumer.getCurrentFrameSession(), mConsumer.getRemovedIds());
        verifyCapturedContent(null, frameId, url, toStringSet(newContent2), capturedContentIds,
                mConsumer.getParentFrame(), mConsumer.getCapturedContent());

        // Remove the element.
        removedContentId = mConsumer.getCapturedContent().getChildren().get(0).getId();
        capturedContentIds = mConsumer.cloneCaptureContentIds();
        runAndVerifyCallbacks(() -> {
            removeElement(newContentId);
        }, toIntArray(TestAwContentCaptureConsumer.CONTENT_REMOVED));
        verifyRemovedContent(frameId, url, toLongSet(removedContentId),
                mConsumer.getCurrentFrameSession(), mConsumer.getRemovedIds());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testChangeContent() throws Throwable {
        final String response = "<html><head></head><body>"
                + "<div id='editable_id'>Hello</div>"
                + "</div></body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);
        runAndVerifyCallbacks(() -> {
            loadUrlSync(url);
        }, toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        Long frameId = null;
        Set<Long> capturedContentIds = null;
        // Verify only on-screen content is captured.
        verifyCapturedContent(null, frameId, url, toStringSet("Hello"), capturedContentIds,
                mConsumer.getParentFrame(), mConsumer.getCapturedContent());

        // Change the content, we shall get content updated callback.
        frameId = Long.valueOf(mConsumer.getCapturedContent().getId());
        capturedContentIds = mConsumer.cloneCaptureContentIds();
        final String changeContent = "Hello world";
        runAndVerifyCallbacks(() -> {
            changeContent("editable_id", changeContent);
        }, toIntArray(TestAwContentCaptureConsumer.CONTENT_UPDATED));
        verifyUpdatedContent(null, frameId, url, toStringSet(changeContent), capturedContentIds,
                mConsumer.getParentFrame(), mConsumer.getUpdatedContent());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRemoveSession() throws Throwable {
        final String response = "<html><head></head><body>"
                + "<div id='editable_id'>Hello</div>"
                + "</div></body></html>";
        final String response2 = "<html><head></head><body>"
                + "<div id='editable_id'>World</div>"
                + "</div></body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);
        final String url2 = mWebServer.setResponse(SECOND_PAGE, response2, null);

        runAndVerifyCallbacks(() -> {
            loadUrlSync(url);
        }, toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));
        Long frameId = null;
        Set<Long> capturedContentIds = null;
        verifyCapturedContent(null, frameId, url, toStringSet("Hello"), capturedContentIds,
                mConsumer.getParentFrame(), mConsumer.getCapturedContent());

        // Keep a copy of current session to verify it removed later.
        FrameSession removedSession = createFrameSession(mConsumer.getCapturedContent());
        capturedContentIds = mConsumer.cloneCaptureContentIds();
        int[] expectedCallbacks = toIntArray(TestAwContentCaptureConsumer.SESSION_REMOVED,
                TestAwContentCaptureConsumer.CONTENT_CAPTURED);
        runAndVerifyCallbacks(() -> { loadUrlSync(url2); }, expectedCallbacks);
        verifyCapturedContent(null, frameId, url2, toStringSet("World"), capturedContentIds,
                mConsumer.getParentFrame(), mConsumer.getCapturedContent());
        // Verify previous session has been removed.
        verifyFrameSession(removedSession, mConsumer.getRemovedSession());

        // Keep a copy of current session to verify it removed later.
        removedSession = createFrameSession(mConsumer.getCapturedContent());
        runAndVerifyCallbacks(() -> {
            destroyAwContents();
        }, toIntArray(TestAwContentCaptureConsumer.SESSION_REMOVED));
        verifyFrameSession(removedSession, mConsumer.getRemovedSession());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRemoveIframe() throws Throwable {
        final String subFrame = "<html><head></head><body>"
                + "<div id='editable_id'>Hello</div>"
                + "</div></body></html>";
        final String subFrameUrl = mWebServer.setResponse(SECOND_PAGE, subFrame, null);
        final String mainFrame = "<html><head></head><body>"
                + "<iframe id='sub_frame_id' src='" + subFrameUrl + "'></iframe></body></html>";
        final String mainFrameUrl = mWebServer.setResponse(MAIN_FRAME_FILE, mainFrame, null);
        runAndVerifyCallbacks(() -> {
            loadUrlSync(mainFrameUrl);
        }, toIntArray(TestAwContentCaptureConsumer.CONTENT_CAPTURED));

        FrameSession expectedParentFrameSession = createFrameSession(mainFrameUrl);
        Long frameId = null;
        verifyCapturedContent(expectedParentFrameSession, frameId, subFrameUrl,
                toStringSet("Hello"), null, mConsumer.getParentFrame(),
                mConsumer.getCapturedContent());

        FrameSession removedSession = createFrameSession(
                mConsumer.getCapturedContent(), mConsumer.getParentFrame().get(0));
        runAndVerifyCallbacks(() -> {
            runScript("var frame = document.getElementById('sub_frame_id');"
                    + "frame.parentNode.removeChild(frame);");
        }, toIntArray(TestAwContentCaptureConsumer.SESSION_REMOVED));
        verifyFrameSession(removedSession, mConsumer.getRemovedSession());
    }
}
