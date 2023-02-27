// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import static org.robolectric.Shadows.shadowOf;

import android.os.Process;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwOriginVerifier;
import org.chromium.android_webview.AwVerificationResultStore;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.content_relationship_verification.OriginVerifier;
import org.chromium.components.content_relationship_verification.OriginVerifier.OriginVerificationListener;
import org.chromium.components.content_relationship_verification.OriginVerifierJni;
import org.chromium.components.content_relationship_verification.OriginVerifierUnitTestSupport;
import org.chromium.components.content_relationship_verification.RelationshipCheckResult;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.CountDownLatch;

/**
 * JUnit tests for AwOriginVerifier.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AwOriginVerifierTest {
    public static final String TEST_BATCH_NAME = "aw_origin_verifier";

    private static final String PACKAGE_NAME = "org.chromium.com";
    private int mUid = Process.myUid();

    private Origin mHttpsOrigin = Origin.create("https://www.example.com");

    private AwOriginVerifier mAwVerifier;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.WARN);

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private OriginVerifier.Natives mMockOriginVerifierJni;

    private static class TestOriginVerificationListener implements OriginVerificationListener {
        private CountDownLatch mLatch;
        private boolean mVerified;

        TestOriginVerificationListener(CountDownLatch latch) {
            mLatch = latch;
        }

        @Override
        public void onOriginVerified(
                String packageName, Origin origin, boolean verified, Boolean online) {
            mVerified = verified;
            mLatch.countDown();
        }

        public boolean isVerified() {
            return mVerified;
        }
    }

    @Before
    public void setUp() throws Exception {
        OriginVerifierUnitTestSupport.registerPackageWithSignature(
                shadowOf(ApplicationProvider.getApplicationContext().getPackageManager()),
                PACKAGE_NAME, mUid);

        mJniMocker.mock(OriginVerifierJni.TEST_HOOKS, mMockOriginVerifierJni);
        Mockito.doAnswer(args -> { return 100L; })
                .when(mMockOriginVerifierJni)
                .init(Mockito.any(), Mockito.any());

        mJniMocker.mock(OriginVerifierJni.TEST_HOOKS, mMockOriginVerifierJni);
        Mockito.doAnswer(args -> {
                   String[] fingerprints = args.getArgument(3);
                   if (fingerprints == null) {
                       mAwVerifier.onOriginVerificationResult(
                               args.getArgument(4), RelationshipCheckResult.FAILURE);
                       return false;
                   }
                   // Ensure parsing of signature works.
                   assert fingerprints.length == 1;
                   assert fingerprints[0] != null;
                   mAwVerifier.onOriginVerificationResult(
                           args.getArgument(4), RelationshipCheckResult.SUCCESS);
                   return true;
               })
                .when(mMockOriginVerifierJni)
                .verifyOrigin(ArgumentMatchers.anyLong(), Mockito.any(),
                        ArgumentMatchers.anyString(), Mockito.any(), ArgumentMatchers.anyString(),
                        ArgumentMatchers.anyString(), Mockito.any());
    }

    @Test
    public void testVerification() throws Exception {
        mAwVerifier = new AwOriginVerifier(PACKAGE_NAME,
                "delegate_permission/common.handle_all_urls", Mockito.mock(AwBrowserContext.class),
                AwVerificationResultStore.getInstance());
        CountDownLatch verificationResultLatch = new CountDownLatch(1);
        TestOriginVerificationListener resultListener =
                new TestOriginVerificationListener(verificationResultLatch);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAwVerifier.start(resultListener, mHttpsOrigin));
        verificationResultLatch.await();
        Assert.assertTrue(resultListener.isVerified());
    }

    @Test
    public void testVerificationResultGetsCached() throws Exception {
        AwVerificationResultStore store = AwVerificationResultStore.getInstance();
        mAwVerifier =
                new AwOriginVerifier(PACKAGE_NAME, "delegate_permission/common.handle_all_urls",
                        Mockito.mock(AwBrowserContext.class), store);
        CountDownLatch verificationResultLatch = new CountDownLatch(1);
        TestOriginVerificationListener resultListener =
                new TestOriginVerificationListener(verificationResultLatch);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAwVerifier.start(resultListener, mHttpsOrigin));
        verificationResultLatch.await();

        Assert.assertTrue(mAwVerifier.checkForSavedResult(mHttpsOrigin));
        Assert.assertTrue(mAwVerifier.wasPreviouslyVerified(mHttpsOrigin));
    }
}