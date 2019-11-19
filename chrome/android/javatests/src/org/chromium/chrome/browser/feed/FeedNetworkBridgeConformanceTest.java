// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.content.Context;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import com.google.android.libraries.feed.api.host.network.HttpRequest;
import com.google.android.libraries.feed.api.host.network.HttpResponse;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.testing.conformance.network.NetworkClientConformanceTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

/**
 * Conformance Tests for {@link FeedNetworkBridge}.
 * The actual tests are implemented in NetworkClientConformanceTest
 */

// The @SmallTest class annotation is needed to allow
// the inherited @Test methods to run using build/android/test_runner.py
@SmallTest
@RunWith(ChromeJUnit4ClassRunner.class)
public final class FeedNetworkBridgeConformanceTest extends NetworkClientConformanceTest {
    private static final String TAG = "FeedConformanceTest";
    private static final long TIMEOUT = scaleTimeout(3000);

    @Rule
    public final ChromeBrowserTestRule mRule = new ChromeBrowserTestRule();

    private EmbeddedTestServer mTestServer;

    class TestConsumer implements Consumer<HttpResponse> {
        private FutureTask<Void> mAcceptFuture;
        private Consumer<HttpResponse> mConsumer;

        public TestConsumer(Consumer<HttpResponse> consumer) {
            mConsumer = consumer;
            mAcceptFuture = new FutureTask<Void>(() -> null);
        }

        @Override
        public void accept(HttpResponse input) {
            mConsumer.accept(input);
            mAcceptFuture.run();
        }

        public void waitUntilCalled(long milliSecsTimeout) {
            try {
                mAcceptFuture.get(milliSecsTimeout, TimeUnit.MILLISECONDS);
            } catch (Exception e) {
                Log.w(TAG, "Exception while waiting for accept: " + e);
                e.printStackTrace();
            }
        }
    }

    class FeedTestNetworkBridge extends FeedNetworkBridge {
        public FeedTestNetworkBridge(Profile p) {
            super(p);
        }

        @Override
        public void send(HttpRequest request, Consumer<HttpResponse> responseConsumer) {
            // TODO(aluo): remove once b/79609987 is fixed
            // The NetworkClientConformanceTest sends requests to google.com,
            // change it to use a local URI in tests.
            String url = mTestServer.getURL("/chrome/test/data/google/google.html");
            Uri uri = Uri.parse(url);
            HttpRequest testServerRequest = new HttpRequest(
                    uri, request.getMethod(), request.getHeaders(), request.getBody());
            TestConsumer testConsumer = new TestConsumer(responseConsumer);
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> super.send(testServerRequest, testConsumer));
            // TODO(aluo): remove once b/79753857 is fixed
            // Need convert the send into a sync call due to
            // NetworkClientConformanceTest not waiting before checking that
            // responseConsumer is accepted.
            testConsumer.waitUntilCalled(TIMEOUT);
        }
    }

    private void createNetworkClient() {
        // The networkClient is declared and tested in NetworkClientConformanceTest
        networkClient = new FeedTestNetworkBridge(Profile.getLastUsedProfile());
    }

    private void destroyNetworkClient() {
        ((FeedTestNetworkBridge) networkClient).destroy();
        networkClient = null;
    }

    private void createAndStartTestServer() {
        Context c = InstrumentationRegistry.getContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(c);
    }

    private void stopAndDestroyTestServer() {
        mTestServer.stopAndDestroyServer();
        mTestServer = null;
    }

    @Before
    public void setUp() {
        createAndStartTestServer();
        TestThreadUtils.runOnUiThreadBlocking(() -> createNetworkClient());
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> destroyNetworkClient());
        stopAndDestroyTestServer();
    }
}
