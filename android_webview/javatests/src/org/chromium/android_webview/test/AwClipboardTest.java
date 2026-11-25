// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static android.content.Context.CLIPBOARD_SERVICE;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.graphics.Color;
import android.os.SystemClock;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.TestWebMessageListener.Data;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.util.TestWebServer;

import java.util.Collections;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/** Tests that the JavaScript clipboard interface works as expected in WebView. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class AwClipboardTest extends AwParameterizedTest {

    private static final String CLIPBOARD_PAGE_HTML =
            """
            <!DOCTYPE html>
            <button id="clip">CLIP!</button>
            <script>
              async function writeClipboardText() {
                try {
                  await navigator.clipboard.writeText("clipped!");
                  resultListener.postMessage("done");
                } catch (error) {
                  resultListener.postMessage(error.message);
                }
              }
              document.getElementById("clip").addEventListener("click", writeClipboardText);
              resultListener.postMessage("loaded");
            </script>
            """;

    private static final String PASTE_PAGE_HTML =
            """
            <!DOCTYPE html>
            <textarea id="editor" autofocus></textarea>
            <script>
              const editor = document.getElementById("editor");
              editor.focus();
              editor.addEventListener("paste", (e) => {
                const text = (e.clipboardData || window.clipboardData).getData("text/plain");
                resultListener.postMessage("pasted:" + text);
              });
              if (document.activeElement === editor) {
                resultListener.postMessage("loaded");
              } else {
                editor.addEventListener("focus", () => {
                  resultListener.postMessage("loaded");
                }, { once: true });
              }
            </script>
            """;

    private static final String READ_CLIPBOARD_PAGE_HTML =
            """
            <!DOCTYPE html>
            <button id="read">READ!</button>
            <script>
              async function readClipboardText() {
                try {
                  const text = await navigator.clipboard.readText();
                  resultListener.postMessage("read:" + text);
                } catch (error) {
                  resultListener.postMessage("error:" + error.name);
                }
              }
              document.getElementById("read").addEventListener("click", readClipboardText);
              resultListener.postMessage("loaded");
            </script>
            """;

    @Rule public AwActivityTestRule mActivityTestRule;
    private AwContents mAwContents;
    private TestWebMessageListener mWebMessageListener;
    private TestWebServer mServer;
    private AwTestContainerView mTestContainerView;

    public AwClipboardTest(AwSettingsMutation params) {
        this.mActivityTestRule = new AwActivityTestRule(params.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mTestContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(new TestAwContentsClient());
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebContents webContents = mTestContainerView.getWebContents();
                    ImeAdapter imeAdapter = ImeAdapter.fromWebContents(webContents);
                    if (imeAdapter != null) {
                        imeAdapter.setInputMethodManagerWrapper(
                                TestInputMethodManagerWrapper.create(imeAdapter));
                    }
                });

        mWebMessageListener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mAwContents, "resultListener", new String[] {"*"}, mWebMessageListener);

        mServer = TestWebServer.start();
        CriteriaHelper.pollUiThread(
                () -> mTestContainerView.hasWindowFocus(),
                "Activity and test container view must have window focus",
                AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @After
    public void tearDown() throws Exception {
        mServer.close();
        clearClipboardOnUiThread();
        ThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().removeAllViews());
    }

    @Test
    @SmallTest
    public void testAutoGrantWithUserGesture() throws Exception {
        String pageUrl = mServer.setResponse("/clip", CLIPBOARD_PAGE_HTML, Collections.emptyList());
        mActivityTestRule.loadUrlAsync(mAwContents, pageUrl);
        Data loadData = mWebMessageListener.waitForOnPostMessage();
        Assert.assertEquals("loaded", loadData.getAsString());
        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "clip");

        Data doneData = mWebMessageListener.waitForOnPostMessage();
        Assert.assertEquals("done", doneData.getAsString());

        String clip = getClipboardTextOnUiThread();
        Assert.assertEquals("clipped!", clip);
    }

    @Test
    @SmallTest
    public void testPasteDeliversClipboardContent() throws Exception {
        setClipboardTextOnUiThread("pasted text content");

        String pageUrl = mServer.setResponse("/paste", PASTE_PAGE_HTML, Collections.emptyList());
        mActivityTestRule.loadUrlAsync(mAwContents, pageUrl);
        Data loadData = mWebMessageListener.waitForOnPostMessage();
        Assert.assertEquals("loaded", loadData.getAsString());

        activateFocus();
        WebContentsUtils.paste(mAwContents.getWebContents());

        Data pasteData = mWebMessageListener.waitForOnPostMessage();
        Assert.assertEquals("pasted:pasted text content", pasteData.getAsString());
    }

    @Test
    @SmallTest
    public void testPasteAsPlainTextDeliversClipboardContent() throws Exception {
        setClipboardTextOnUiThread("plain pasted content");

        String pageUrl =
                mServer.setResponse("/paste_plain", PASTE_PAGE_HTML, Collections.emptyList());
        mActivityTestRule.loadUrlAsync(mAwContents, pageUrl);
        Data loadData = mWebMessageListener.waitForOnPostMessage();
        Assert.assertEquals("loaded", loadData.getAsString());

        activateFocus();
        WebContentsUtils.pasteAsPlainText(mAwContents.getWebContents());

        Data pasteData = mWebMessageListener.waitForOnPostMessage();
        Assert.assertEquals("pasted:plain pasted content", pasteData.getAsString());
    }

    @Test
    @SmallTest
    public void testProgrammaticReadDeniedEvenWithUserGesture() throws Exception {
        setClipboardTextOnUiThread("secret clipboard data");

        String pageUrl =
                mServer.setResponse(
                        "/read_clip", READ_CLIPBOARD_PAGE_HTML, Collections.emptyList());
        mActivityTestRule.loadUrlAsync(mAwContents, pageUrl);
        Data loadData = mWebMessageListener.waitForOnPostMessage();
        Assert.assertEquals("loaded", loadData.getAsString());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "read");

        Data resultData = mWebMessageListener.waitForOnPostMessage();
        Assert.assertEquals("error:NotAllowedError", resultData.getAsString());
    }

    private void activateFocus() throws Exception {
        waitForHardwareFrameReady();
        CriteriaHelper.pollUiThread(
                () -> mTestContainerView.hasWindowFocus(),
                "Activity and test container view must have window focus",
                AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewEventSink.from(mAwContents.getWebContents())
                            .onActivityTopResumedChangedForTesting(true);
                    mTestContainerView.requestFocus();
                    mAwContents.getViewMethods().onWindowFocusChanged(true);
                    mAwContents.getViewMethods().onFocusChanged(true, 0, null);
                    mAwContents.getWebContents().setFocus(true);
                });
        CriteriaHelper.pollUiThread(
                () -> mTestContainerView.hasFocus(),
                "Test container view must have view focus",
                AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        ImeAdapter imeAdapter =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> ImeAdapter.fromWebContents(mAwContents.getWebContents()));
        CriteriaHelper.pollUiThread(
                () ->
                        imeAdapter != null
                                && imeAdapter.focusedNodeEditable()
                                && mAwContents.getWebContents().isFocusedElementEditable()
                                && mAwContents.getWebContents().getFocusedFrame() != null,
                "Focused node must be editable and frame must have focus",
                AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void waitForHardwareFrameReady() throws Exception {
        if (!mTestContainerView.isBackedByHardwareView()) {
            return;
        }
        // The first AwTestContainerView in a process uses HardwareView, whose initial draw on
        // RenderThreadInstr performs cold HardwareRenderer/GL initialization that can take >5s on
        // x64 emulators. Do not call waitForVisualStateCallback() beforehand because its 5-second
        // CallbackHelper timeout will expire if AwContents.onDraw() blocks the Browser UI thread on
        // that cold RenderThreadInstr draw. Instead, wait directly with SCALED_WAIT_TIMEOUT_MS for
        // that hardware draw and any queued RenderThreadInstr work to complete before triggering
        // paste.
        long deadline = SystemClock.uptimeMillis() + AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS;
        LinkedBlockingQueue<int[]> colorQueue = new LinkedBlockingQueue<>();
        while (SystemClock.uptimeMillis() < deadline) {
            // Wait for a render callback.
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTestContainerView.invalidate();
                        mTestContainerView.readbackQuadrantColors(colorQueue::add);
                    });

            int[] colors =
                    colorQueue.poll(
                            AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            Assert.assertNotNull("Waiting for readbackQuadrantColors timed out", colors);
            Assert.assertEquals(4, colors.length);
            // Check that the lower-right color is opaque. The colors will be transparent (alpha =
            // 0) until the hardware has actually rendered for the first time.
            boolean hasRenderedFrame = Color.alpha(colors[3]) == 255;
            if (hasRenderedFrame) {
                // Wait for a second frame to be draw, to ensure that we only return after any other
                // render callbacks and onDraw methods have returned.
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mTestContainerView.readbackQuadrantColors(colorQueue::add));
                Assert.assertNotNull(
                        "Waiting for readbackQuadrantColors timed out",
                        colorQueue.poll(
                                AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
                return;
            }
            Thread.sleep(50);
        }
        Assert.fail("Timed out waiting for hardware frame to finish rendering");
    }

    private ClipboardManager getClipboardManager() {
        Context context = mTestContainerView.getContext();
        return (ClipboardManager) context.getSystemService(CLIPBOARD_SERVICE);
    }

    public void setClipboardTextOnUiThread(String text) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ClipboardManager clipboard = getClipboardManager();
                    clipboard.setPrimaryClip(ClipData.newPlainText("text", text));
                });
        CriteriaHelper.pollUiThread(
                () -> text.equals(getClipboardTextOnUiThread()),
                "Clipboard content did not update to: " + text,
                AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Nullable
    public String getClipboardTextOnUiThread() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ClipboardManager clipboard = getClipboardManager();
                    ClipData clip = clipboard.getPrimaryClip();
                    if (clip == null) {
                        return null;
                    }
                    if (clip.getItemCount() < 1) {
                        return null;
                    }
                    Context context = mTestContainerView.getContext();
                    return String.valueOf(clip.getItemAt(0).coerceToText(context));
                });
    }

    public void clearClipboardOnUiThread() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ClipboardManager clipboard = getClipboardManager();
                    clipboard.clearPrimaryClip();
                });
        CriteriaHelper.pollUiThread(
                () -> getClipboardTextOnUiThread() == null,
                "Clipboard was not cleared",
                AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}
