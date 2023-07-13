// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.environment_integrity;

import android.util.Base64;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.environment_integrity.IntegrityException;
import org.chromium.components.environment_integrity.IntegrityServiceBridge;
import org.chromium.components.environment_integrity.IntegrityServiceBridgeDelegate;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;
import java.util.Queue;

/** Test suite for navigator.getEnvironmentIntegrity functionality. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
        "enable-features=WebEnvironmentIntegrity", "ignore-certificate-errors"})
@Batch(Batch.PER_CLASS)
public class EnvironmentIntegrityTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    private static final String TEST_HOSTNAME = "test.host";
    private static final long HANDLE = 123456789L;
    private static final long ALTERNATE_HANDLE = 987654321L;

    private static final byte[] TOKEN = {0xa, 0xb, 0xc, 0xd};
    private static final byte[] ALTERNATE_TOKEN = {0xf, 0xe, 0xd, 0xc};

    private static final String ENCODED_TOKEN = Base64.encodeToString(TOKEN, Base64.NO_WRAP);
    private static final String ALTERNATE_ENCODED_TOKEN =
            Base64.encodeToString(ALTERNATE_TOKEN, Base64.NO_WRAP);

    private static final String EXPECTED_UNKNOWN_ERROR = "UnknownError: Unknown Error.";
    private static final String EXPECTED_TIMEOUT_ERROR = "TimeoutError: Request Timeout.";

    private static final String TEST_FILE = "/chrome/test/data/android/environment_integrity.html";
    private String mUrl;
    private Tab mTab;
    private EnvironmentIntegrityUpdateWaiter mUpdateWaiter;
    private final TestIntegrityServiceBridge mIntegrityDelegate = new TestIntegrityServiceBridge();

    /**
     * Helper class to pass parsed title string.
     */
    private static class ExecutionResult {
        public ExecutionResult(boolean success, String message) {
            this.success = success;
            this.message = message;
        }

        public final boolean success;
        public final String message;
    }

    /** Waits until the JavaScript code supplies a result through the window title. */
    private class EnvironmentIntegrityUpdateWaiter extends EmptyTabObserver {
        private final CallbackHelper mCallbackHelper = new CallbackHelper();
        private String mStatus;

        public EnvironmentIntegrityUpdateWaiter() {}

        @Override
        public void onTitleUpdated(Tab tab) {
            String title = mActivityTestRule.getActivity().getActivityTab().getTitle();
            // Wait until the title indicates either success or failure.
            if (!title.startsWith("Done|")) return;
            mStatus = title;
            mCallbackHelper.notifyCalled();
        }

        public ExecutionResult waitForResult() throws Exception {
            mCallbackHelper.waitForNext();
            String[] parts = mStatus.split("\\|");
            return new ExecutionResult("Success".equals(parts[1]), parts[2]);
        }
    }

    /**
     * Class to store parameters passed to attester for token requests.
     */
    private static class TokenRequest {
        public TokenRequest(long handle, byte[] requestHash) {
            this.handle = handle;
            this.requestHash = requestHash;
        }

        public final long handle;
        public final byte[] requestHash;
    }

    /**
     * Test implementation of IntegrityServiceBridge to control attester behavior.
     */
    private static class TestIntegrityServiceBridge implements IntegrityServiceBridgeDelegate {
        private boolean mCanUseGms;

        void setCanUseGms(boolean canUseGms) {
            mCanUseGms = canUseGms;
        }

        Queue<ListenableFuture<Long>> mHandleFutures = new LinkedList<>();
        Queue<ListenableFuture<byte[]>> mTokenFutures = new LinkedList<>();

        final List<Boolean> mHandleRequests = new ArrayList<>();
        final List<TokenRequest> mTokenRequests = new ArrayList<>();

        void addHandleFuture(ListenableFuture<Long> future) {
            mHandleFutures.offer(future);
        }

        void addTokenFuture(ListenableFuture<byte[]> future) {
            mTokenFutures.offer(future);
        }

        public List<TokenRequest> getTokenRequests() {
            return mTokenRequests;
        }

        public List<Boolean> getHandleRequests() {
            return mHandleRequests;
        }

        @Override
        public ListenableFuture<Long> createEnvironmentIntegrityHandle(
                boolean bindAppIdentity, int timeoutMilliseconds) {
            Assert.assertFalse(mHandleFutures.isEmpty());
            mHandleRequests.add(bindAppIdentity);
            return mHandleFutures.poll();
        }

        @Override
        public ListenableFuture<byte[]> getEnvironmentIntegrityToken(
                long handle, byte[] requestHash, int timeoutMilliseconds) {
            Assert.assertFalse(mTokenFutures.isEmpty());
            mTokenRequests.add(new TokenRequest(handle, requestHash));
            return mTokenFutures.poll();
        }

        @Override
        public boolean canUseGms() {
            return mCanUseGms;
        }
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        EmbeddedTestServer mTestServer = mActivityTestRule.getTestServer();
        mUrl = mTestServer.getURLWithHostName(TEST_HOSTNAME, TEST_FILE);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        mUpdateWaiter = new EnvironmentIntegrityUpdateWaiter();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            IntegrityServiceBridge.setDelegateForTesting(mIntegrityDelegate);
            mTab.addObserver(mUpdateWaiter);
        });
    }

    /**
     * Verify that navigator.getEnvironmentIntegrity succeeds.
     */
    @Test
    @MediumTest
    public void testGetEnvironmentIntegrity() throws Exception {
        mIntegrityDelegate.setCanUseGms(true);
        mIntegrityDelegate.addHandleFuture(Futures.immediateFuture(HANDLE));
        mIntegrityDelegate.addTokenFuture(Futures.immediateFuture(TOKEN));

        Origin origin = Origin.create(new GURL(mUrl));
        Assert.assertEquals("https", origin.getScheme());
        Assert.assertEquals(TEST_HOSTNAME, origin.getHost());

        final String contentBinding = "contentBinding";
        final ExecutionResult result = runDoGetEnvironmentIntegrity(contentBinding);

        Assert.assertTrue(result.success);
        Assert.assertEquals(ENCODED_TOKEN, result.message);
        final List<TokenRequest> tokenRequests = mIntegrityDelegate.getTokenRequests();
        Assert.assertEquals(1, tokenRequests.size());
        Assert.assertEquals(HANDLE, tokenRequests.get(0).handle);

        Assert.assertArrayEquals(expectedHash(contentBinding), tokenRequests.get(0).requestHash);
    }

    @Test
    @MediumTest
    public void testNoGmsResultsInUnknownError() throws Exception {
        mIntegrityDelegate.setCanUseGms(false);

        final String contentBinding = "contentBinding";
        final ExecutionResult result = runDoGetEnvironmentIntegrity(contentBinding);

        // Should not attempt to request a handle or token.
        Assert.assertEquals(0, mIntegrityDelegate.getHandleRequests().size());
        Assert.assertEquals(0, mIntegrityDelegate.getTokenRequests().size());

        Assert.assertFalse(result.success);
        Assert.assertEquals(EXPECTED_UNKNOWN_ERROR, result.message);
    }

    @Test
    @MediumTest
    public void testTimeoutResultsInTimeoutException() throws Exception {
        mIntegrityDelegate.setCanUseGms(true);
        mIntegrityDelegate.addHandleFuture(Futures.immediateFailedFuture(new IntegrityException("",
                org.chromium.components.environment_integrity.enums.IntegrityResponse.TIMEOUT)));

        final String contentBinding = "contentBinding";
        final ExecutionResult result = runDoGetEnvironmentIntegrity(contentBinding);
        // Should result in handle being requested, but not token.
        Assert.assertEquals(1, mIntegrityDelegate.getHandleRequests().size());
        Assert.assertEquals(0, mIntegrityDelegate.getTokenRequests().size());

        Assert.assertFalse(result.success);
        Assert.assertEquals(EXPECTED_TIMEOUT_ERROR, result.message);
    }

    @Test
    @MediumTest
    public void testHandleIsAutoRefreshed() throws Exception {
        mIntegrityDelegate.setCanUseGms(true);
        // Need two handle responses.
        mIntegrityDelegate.addHandleFuture(Futures.immediateFuture(HANDLE));
        mIntegrityDelegate.addHandleFuture(Futures.immediateFuture(ALTERNATE_HANDLE));

        // First token request should abort with invalid handle - we should get a second request.
        mIntegrityDelegate.addTokenFuture(Futures.immediateFailedFuture(
                new IntegrityException("First request uses invalid handle",
                        org.chromium.components.environment_integrity.enums.IntegrityResponse
                                .INVALID_HANDLE)));
        mIntegrityDelegate.addTokenFuture(Futures.immediateFuture(TOKEN));

        final String contentBinding = "contentBinding";
        final ExecutionResult result = runDoGetEnvironmentIntegrity(contentBinding);

        Assert.assertTrue(result.success);
        Assert.assertEquals(ENCODED_TOKEN, result.message);

        Assert.assertEquals(2, mIntegrityDelegate.getHandleRequests().size());

        final List<TokenRequest> tokenRequests = mIntegrityDelegate.getTokenRequests();
        Assert.assertEquals(2, tokenRequests.size());
        Assert.assertEquals(HANDLE, tokenRequests.get(0).handle);
        Assert.assertEquals(ALTERNATE_HANDLE, tokenRequests.get(1).handle);

        byte[] hash = expectedHash(contentBinding);
        Assert.assertArrayEquals(hash, tokenRequests.get(0).requestHash);
        Assert.assertArrayEquals(hash, tokenRequests.get(1).requestHash);
    }

    @Test
    @MediumTest
    public void testHandleIsReused() throws Exception {
        mIntegrityDelegate.setCanUseGms(true);
        // Only need one handle request.
        mIntegrityDelegate.addHandleFuture(Futures.immediateFuture(HANDLE));

        // Use scopes to avoid mixing variables between the two requests.

        {
            // First request
            mIntegrityDelegate.addTokenFuture(Futures.immediateFuture(TOKEN));
            final String contentBinding = "contentBinding";
            final ExecutionResult result = runDoGetEnvironmentIntegrity(contentBinding);

            Assert.assertTrue(result.success);
            Assert.assertEquals(ENCODED_TOKEN, result.message);

            Assert.assertEquals(1, mIntegrityDelegate.getHandleRequests().size());
            Assert.assertEquals(1, mIntegrityDelegate.getTokenRequests().size());
            Assert.assertEquals(HANDLE, mIntegrityDelegate.getTokenRequests().get(0).handle);
        }

        {
            // Second request
            mIntegrityDelegate.addTokenFuture(Futures.immediateFuture(ALTERNATE_TOKEN));
            final String contentBinding2 = "contentBinding2";
            final ExecutionResult result2 = runDoGetEnvironmentIntegrity(contentBinding2);
            Assert.assertTrue(result2.success);
            Assert.assertEquals(ALTERNATE_ENCODED_TOKEN, result2.message);

            // We still only expect a single handle request, as the handle should have been reused.
            Assert.assertEquals(1, mIntegrityDelegate.getHandleRequests().size());

            // We expect that a new request for a token was made.
            Assert.assertEquals(2, mIntegrityDelegate.getTokenRequests().size());
            // We expect the handle was re-used.
            Assert.assertEquals(HANDLE, mIntegrityDelegate.getTokenRequests().get(1).handle);
            // We expect the requestHash to match the content binding used.
            Assert.assertArrayEquals(expectedHash(contentBinding2),
                    mIntegrityDelegate.getTokenRequests().get(1).requestHash);
        }
    }

    @Test
    @MediumTest
    public void testNullContentBinding() throws Exception {
        mIntegrityDelegate.setCanUseGms(true);
        mIntegrityDelegate.addHandleFuture(Futures.immediateFuture(HANDLE));
        mIntegrityDelegate.addTokenFuture(Futures.immediateFuture(TOKEN));

        // Passing explicit `null`.
        ExecutionResult result = runDoGetEnvironmentIntegrity(null);
        // The flow completes.
        Assert.assertTrue(result.success);
        Assert.assertEquals(ENCODED_TOKEN, result.message);

        // The content binding is equivalent to passing the string "null".
        Assert.assertArrayEquals(
                expectedHash("null"), mIntegrityDelegate.getTokenRequests().get(0).requestHash);
    }

    @NonNull
    private byte[] expectedHash(@NonNull String contentBinding) throws NoSuchAlgorithmException {
        // The browser postfixes the content binding with the eTLD+1 and ';' as separator.
        // Do the same here, to ensure test values match.
        String schemefulSite = "https://" + TEST_HOSTNAME;
        String hashedBinding = contentBinding + ";" + schemefulSite;
        return MessageDigest.getInstance("SHA-256").digest(
                hashedBinding.getBytes(StandardCharsets.UTF_8));
    }

    @NonNull
    private ExecutionResult runDoGetEnvironmentIntegrity(@Nullable String contentBinding)
            throws Exception {
        mActivityTestRule.loadUrl(mUrl);
        if (contentBinding == null) {
            mActivityTestRule.runJavaScriptCodeInCurrentTab("doGetEnvironmentIntegrity(null)");
        } else {
            mActivityTestRule.runJavaScriptCodeInCurrentTab(
                    "doGetEnvironmentIntegrity('" + contentBinding + "')");
        }
        return mUpdateWaiter.waitForResult();
    }
}
