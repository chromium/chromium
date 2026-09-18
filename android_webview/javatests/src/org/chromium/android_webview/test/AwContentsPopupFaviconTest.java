// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

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
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.net.test.util.TestWebServer;

import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class AwContentsPopupFaviconTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mParentContentsClient;
    private AwContents mParentContents;
    private TestWebServer mWebServer;
    private ServerSocket mTarpitServerSocket;
    private Thread mAcceptThread;
    private final List<Socket> mTarpitConnections = new ArrayList<>();
    private int mTarpitPort;

    public AwContentsPopupFaviconTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mParentContentsClient = new TestAwContentsClient();
        AwTestContainerView parentContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mParentContentsClient);
        mParentContents = parentContainerView.getAwContents();
        mWebServer = TestWebServer.start();
        startTarpit();
    }

    @After
    public void tearDown() throws Exception {
        if (mWebServer != null) mWebServer.shutdown();
        if (mTarpitServerSocket != null) mTarpitServerSocket.close();
        // Unblock the accept thread's read() on each held connection so the
        // thread can exit, then wait for it.
        synchronized (mTarpitConnections) {
            for (Socket socket : mTarpitConnections) {
                try {
                    socket.close();
                } catch (IOException e) {
                    // Already closed; nothing to do.
                }
            }
            mTarpitConnections.clear();
        }
        if (mAcceptThread != null) {
            mAcceptThread.join(TimeUnit.SECONDS.toMillis(5));
        }
    }

    // A tarpit: accepts connections but never responds, so the popup's
    // navigation hangs indefinitely. The hanging load keeps the popup pending
    // (with no AwSettings attached) until the test adopts it.
    private void startTarpit() throws IOException {
        mTarpitServerSocket = new ServerSocket(0);
        mTarpitPort = mTarpitServerSocket.getLocalPort();
        mAcceptThread =
                new Thread(
                        () -> {
                            try {
                                while (true) {
                                    Socket socket = mTarpitServerSocket.accept();
                                    synchronized (mTarpitConnections) {
                                        mTarpitConnections.add(socket);
                                    }
                                    try {
                                        // Hold the connection open without ever
                                        // responding.
                                        socket.getInputStream().read();
                                    } catch (IOException e) {
                                        // Connection closed during teardown.
                                    }
                                }
                            } catch (IOException e) {
                                // Server socket closed during teardown.
                            }
                        });
        mAcceptThread.setDaemon(true);
        mAcceptThread.start();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures(AwFeatures.WEBVIEW_DOWNLOAD_FAVICONS)
    public void testPopupFaviconUpdateBeforeAdoptionDoesNotCrash() throws Throwable {
        AwActivityTestRule.enableJavaScriptOnUiThread(mParentContents);
        mParentContents.getSettings().setSupportMultipleWindows(true);
        mParentContents.getSettings().setJavaScriptCanOpenWindowsAutomatically(true);

        // Relay for the popup's handshake: the pending popup has no Java
        // object, so its script postMessages the parent page, which forwards
        // the message into Java via the injected "handshake" object.
        TestWebMessageListener handshakeListener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mParentContents, "handshake", new String[] {"*"}, handshakeListener);

        final String parentUrl =
                mWebServer.setResponse(
                        "/popupFaviconParent.html",
                        CommonResources.makeHtmlPageFrom(
                                """
                                <script>
                                window.addEventListener('message', function(e) {
                                    handshake.postMessage(e.data);
                                });
                                </script>
                                """,
                                "parent"),
                        null);
        mParentContentsClient.getOnCreateWindowHelper().setReturnValue(true);
        mActivityTestRule.loadUrlSync(
                mParentContents, mParentContentsClient.getOnPageFinishedHelper(), parentUrl);

        // Open a hanging URL, then synchronously document.write() an icon link
        // plus a handshake script into the provisional load. The parser
        // processes the link (sending the favicon update) before executing the
        // trailing script, so the handshake proves the favicon IPC was sent
        // first.
        final String tarpitUrl = "http://127.0.0.1:" + mTarpitPort + "/spoof-target";
        final String popupIconUrl = "http://127.0.0.1:" + mTarpitPort + "/popup_favicon.png";
        TestAwContentsClient.OnCreateWindowHelper onCreateWindowHelper =
                mParentContentsClient.getOnCreateWindowHelper();
        int createWindowCallCount = onCreateWindowHelper.getCallCount();
        final String openAndWrite =
                "var w = window.open('"
                        + tarpitUrl
                        + "');"
                        + "w.document.write('<link rel=\"icon\" href=\""
                        + popupIconUrl
                        + "\">popup<script>opener.postMessage(\"popup-ready\", \"*\");</script>');";
        ThreadUtils.runOnUiThreadBlocking(
                () -> mParentContents.evaluateJavaScriptForTests(openAndWrite, null));
        onCreateWindowHelper.waitForCallback(
                createWindowCallCount,
                1,
                AwActivityTestRule.WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);

        // Gate adoption on the popup's handshake instead of a fixed sleep: the
        // favicon update was sent before the handshake, so once this returns
        // the pending native AwContents has already seen the favicon IPC.
        TestWebMessageListener.Data handshake = handshakeListener.waitForOnPostMessage();
        Assert.assertEquals("popup-ready", handshake.getAsString());

        // No explicit assertion beyond the handshake: the favicon IPC was
        // queued ahead of it on the same channel, so by this point the browser
        // has processed a favicon update for the still-pending popup. Pre-fix
        // that crashes the browser; post-fix the test simply returns.
    }
}
