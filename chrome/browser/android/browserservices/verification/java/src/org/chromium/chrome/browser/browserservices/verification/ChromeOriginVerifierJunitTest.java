// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import static org.robolectric.Shadows.shadowOf;

import android.os.Process;

import androidx.browser.customtabs.CustomTabsService;
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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.content_relationship_verification.OriginVerifier;
import org.chromium.components.content_relationship_verification.OriginVerifier.OriginVerificationListener;
import org.chromium.components.content_relationship_verification.OriginVerifierJni;
import org.chromium.components.content_relationship_verification.OriginVerifierUnitTestSupport;
import org.chromium.components.content_relationship_verification.RelationshipCheckResult;
import org.chromium.components.embedder_support.util.Origin;

import java.util.concurrent.CountDownLatch;

/** Robolectric tests for ChromeOriginVerifier. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(ChromeOriginVerifierJunitTest.TEST_BATCH_NAME)
public class ChromeOriginVerifierJunitTest {
    public static final String TEST_BATCH_NAME = "chrome_origin_verifier";

    private static final String PACKAGE_NAME = "org.chromium.com";
    private int mUid = Process.myUid();
    private Origin mHttpsOrigin = Origin.create("https://www.example.com");

    private ChromeOriginVerifier mChromeVerifier;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.WARN);

    @Mock private Profile mProfile;

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private OriginVerifier.Natives mMockOriginVerifierJni;

    @Mock private ChromeOriginVerifier.Natives mMockChromeOriginVerifierJni;

    private CountDownLatch mVerificationResultLatch = new CountDownLatch(1);

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
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        OriginVerifierUnitTestSupport.registerPackageWithSignature(
                shadowOf(ApplicationProvider.getApplicationContext().getPackageManager()),
                PACKAGE_NAME,
                mUid);

        mJniMocker.mock(ChromeOriginVerifierJni.TEST_HOOKS, mMockChromeOriginVerifierJni);
        Mockito.doAnswer(
                        args -> {
                            return 100L;
                        })
                .when(mMockChromeOriginVerifierJni)
                .init(Mockito.any(), Mockito.any());

        mJniMocker.mock(OriginVerifierJni.TEST_HOOKS, mMockOriginVerifierJni);
        Mockito.doAnswer(
                        args -> {
                            String[] fingerprints = args.getArgument(3);
                            if (fingerprints == null) {
                                mChromeVerifier.onOriginVerificationResult(
                                        args.getArgument(4), RelationshipCheckResult.FAILURE);
                                return false;
                            }
                            // Ensure parsing of signature works.
                            assert fingerprints.length == 1;
                            assert fingerprints[0] != null;
                            mChromeVerifier.onOriginVerificationResult(
                                    args.getArgument(4), RelationshipCheckResult.SUCCESS);
                            return true;
                        })
                .when(mMockOriginVerifierJni)
                .verifyOrigin(
                        ArgumentMatchers.anyLong(),
                        Mockito.any(),
                        ArgumentMatchers.anyString(),
                        Mockito.any(),
                        ArgumentMatchers.anyString(),
                        ArgumentMatchers.anyString(),
                        Mockito.any());
    }

    @Test
    public void testValidFingerprint() throws Exception {
        mChromeVerifier =
                new ChromeOriginVerifier(
                        PACKAGE_NAME,
                        CustomTabsService.RELATION_HANDLE_ALL_URLS,
                        null,
                        null,
                        ChromeVerificationResultStore.getInstance());
        TestOriginVerificationListener resultListener =
                new TestOriginVerificationListener(mVerificationResultLatch);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mChromeVerifier.start(resultListener, mHttpsOrigin));
        mVerificationResultLatch.await();
        Assert.assertTrue(resultListener.isVerified());
    }

    @Test
    public void testNoFingerprintDoesNotRaise() throws Exception {
        // Remove package from PackageUtils so that {@link
        // PackageUtils#getCertificateSHA256FingerprintForPackage} returns null.
        shadowOf(ApplicationProvider.getApplicationContext().getPackageManager())
                .removePackage(PACKAGE_NAME);
        mChromeVerifier =
                new ChromeOriginVerifier(
                        PACKAGE_NAME,
                        CustomTabsService.RELATION_HANDLE_ALL_URLS,
                        null,
                        null,
                        ChromeVerificationResultStore.getInstance());
        TestOriginVerificationListener resultListener =
                new TestOriginVerificationListener(mVerificationResultLatch);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mChromeVerifier.start(resultListener, mHttpsOrigin));
        mVerificationResultLatch.await();
        Assert.assertFalse(resultListener.isVerified());
    }
}
