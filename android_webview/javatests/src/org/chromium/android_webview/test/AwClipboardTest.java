// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static android.content.Context.CLIPBOARD_SERVICE;

import android.annotation.SuppressLint;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;

import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
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
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.test.TestWebMessageListener.Data;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.net.test.util.TestWebServer;

import java.util.Collections;

/** Tests that the JavaScript clipboard interface works as expected in WebView. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@CommandLineFlags.Add({"enable-features=" + ContentFeatures.WEB_PERMISSIONS_API})
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

        mWebMessageListener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mAwContents, "resultListener", new String[] {"*"}, mWebMessageListener);

        mServer = TestWebServer.start();
    }

    @After
    public void tearDown() throws Exception {
        mServer.close();
        ThreadUtils.runOnUiThreadBlocking(this::clearClipboardOnUiThread);
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_AUTO_GRANT_SANITIZED_CLIPBOARD_WRITE
    })
    public void testAutoGrantWithUserGesture() throws Exception {
        testClipboardWriteWorks();
    }

    /**
     * This test asserts that the legacy behavior still works as long as the kill-switch flag
     * exists.
     */
    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "disable-features=" + AwFeatures.WEBVIEW_AUTO_GRANT_SANITIZED_CLIPBOARD_WRITE
    })
    public void testAutoGrantWithUserGesture_legacy() throws Exception {
        testClipboardWriteWorks();
    }

    @SuppressLint("VisibleForTests")
    private void testClipboardWriteWorks() throws Exception {
        String pageUrl = mServer.setResponse("/clip", CLIPBOARD_PAGE_HTML, Collections.emptyList());
        mActivityTestRule.loadUrlAsync(mAwContents, pageUrl);
        Data loadData = mWebMessageListener.waitForOnPostMessage();
        Assert.assertEquals("loaded", loadData.getAsString());
        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "clip");

        Data doneData = mWebMessageListener.waitForOnPostMessage();
        Assert.assertEquals("done", doneData.getAsString());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    String clip = getClipboardTextOnUiThread();
                    Assert.assertEquals("clipped!", clip);
                });
    }

    @UiThread
    @Nullable
    public String getClipboardTextOnUiThread() {
        Context context = mTestContainerView.getContext();
        android.content.ClipboardManager clipboard =
                (android.content.ClipboardManager) context.getSystemService(CLIPBOARD_SERVICE);
        ClipData clip = clipboard.getPrimaryClip();
        if (clip == null) {
            return null;
        }
        if (clip.getItemCount() < 1) {
            return null;
        }
        return String.valueOf(clip.getItemAt(0).coerceToText(context));
    }

    @UiThread
    public void clearClipboardOnUiThread() {
        Context context = mTestContainerView.getContext();
        ClipboardManager clipboard = (ClipboardManager) context.getSystemService(CLIPBOARD_SERVICE);
        if (VERSION.SDK_INT >= VERSION_CODES.P) {
            clipboard.clearPrimaryClip();
        }
    }
}
