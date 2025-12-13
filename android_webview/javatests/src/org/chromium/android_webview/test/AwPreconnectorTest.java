// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertThrows;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.url.GURL;

import java.io.IOException;
import java.net.ServerSocket;
import java.net.SocketException;
import java.util.concurrent.TimeoutException;

@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class AwPreconnectorTest extends AwParameterizedTest {
    @Rule public MultiProfileTestRule mTestRule;
    private ServerThread mServerThread;

    public AwPreconnectorTest(AwSettingsMutation param) {
        mTestRule = new MultiProfileTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mServerThread = new ServerThread();
        mServerThread.start();
    }

    @After
    public void tearDown() throws Exception {
        if (mServerThread != null) mServerThread.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void connects() throws Exception {
        AwBrowserContext profile = mTestRule.getProfileSync("Default", /* createIfNeeded= */ true);
        String url = "http://localhost:" + mServerThread.getPort();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    profile.getPreconnector().preconnect(new GURL(url));
                });

        mServerThread.waitForConnection();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void connects_multiple() throws Exception {
        AwBrowserContext profile = mTestRule.getProfileSync("Default", /* createIfNeeded= */ true);

        ServerThread serverThread2 = new ServerThread();
        serverThread2.start();

        try {
            String url1 = "http://localhost:" + mServerThread.getPort();
            String url2 = "http://localhost:" + serverThread2.getPort();

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        profile.getPreconnector().preconnect(new GURL(url1));
                        profile.getPreconnector().preconnect(new GURL(url2));
                    });

            mServerThread.waitForConnection();
            serverThread2.waitForConnection();
        } finally {
            serverThread2.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void connects_invalidUrl() throws Exception {
        AwBrowserContext profile = mTestRule.getProfileSync("Default", /* createIfNeeded= */ true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertThrows(
                            IllegalArgumentException.class,
                            () -> profile.getPreconnector().preconnect(new GURL("invalid")));
                    // Note: We require the scheme (so https://www.example.com).
                    assertThrows(
                            IllegalArgumentException.class,
                            () ->
                                    profile.getPreconnector()
                                            .preconnect(new GURL("www.example.com")));
                });
    }

    // A Server that waits for a single connection to be opened, then closes.
    // We can't use the standard test servers because they all expect some HTTP content to be sent,
    // but for preconnect we just open the connection.
    // Copied from net.test.util.WebServer, then drastically cut down.
    private static class ServerThread extends Thread {
        private static final String TAG = "ServerThread";

        private final ServerSocket mSocket;
        private final CallbackHelper mCallbackHelper = new CallbackHelper();

        public ServerThread() throws IOException {
            super("ServerThread");
            // Passing zero as the port will automatically allocate one for us.
            mSocket = new ServerSocket(0);
        }

        public int getPort() {
            return mSocket.getLocalPort();
        }

        public void shutdown() throws IOException {
            try {
                mSocket.close();
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }

        public void waitForConnection() throws TimeoutException {
            mCallbackHelper.waitForCallback(0);
        }

        @Override
        public void run() {
            try {
                mSocket.accept();
                mCallbackHelper.notifyCalled();
                mSocket.close();
            } catch (SocketException e) {
                // This gets thrown if mSocket.close() is called while mSocket.accept is waiting.
                // So this isn't an error here, but waitForConnection will throw instead.
            } catch (IOException e) {
                Log.e(TAG, e.getMessage());
            }
        }
    }
}
