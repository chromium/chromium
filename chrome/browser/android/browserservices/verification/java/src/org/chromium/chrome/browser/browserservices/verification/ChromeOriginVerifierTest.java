// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import android.util.Pair;

import androidx.browser.customtabs.CustomTabsService;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.content_relationship_verification.OriginVerifier.OriginVerificationListener;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.content_public.browser.test.mock.MockWebContents;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Tests for ChromeOriginVerifier. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(ChromeOriginVerifierTest.TEST_BATCH_NAME)
public class ChromeOriginVerifierTest {
    public static final String TEST_BATCH_NAME = "chrome_origin_verifier";

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final long TIMEOUT_MS = 1000;

    private static final String PACKAGE_NAME =
            ContextUtils.getApplicationContext().getPackageName();

    private Origin mHttpsOrigin;
    private Origin mHttpOrigin;
    private TestExternalAuthUtils mExternalAuthUtils;

    private static class TestExternalAuthUtils extends ExternalAuthUtils {
        private final List<Pair<String, Origin>> mAllowlist = new ArrayList<>();

        public void addToAllowlist(String packageName, Origin origin) {
            mAllowlist.add(Pair.create(packageName, origin));
        }

        @Override
        public boolean isAllowlistedForTwaVerification(String packageName, Origin origin) {
            return mAllowlist.contains(Pair.create(packageName, origin));
        }
    }

    private class TestOriginVerificationListener implements OriginVerificationListener {
        @Override
        public void onOriginVerified(
                String packageName, Origin origin, boolean verified, Boolean online) {
            mLastPackageName = packageName;
            mLastOrigin = origin;
            mLastVerified = verified;
            mVerificationResultSemaphore.release();
        }
    }

    private Semaphore mVerificationResultSemaphore;
    private ChromeOriginVerifier mUseAsOriginVerifier;
    private ChromeOriginVerifier mHandleAllUrlsVerifier;
    private volatile String mLastPackageName;
    private volatile Origin mLastOrigin;
    private volatile boolean mLastVerified;

    private boolean verify(ChromeOriginVerifier verifier, Origin origin)
            throws InterruptedException {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> verifier.start(new TestOriginVerificationListener(), origin));
        Assert.assertTrue(
                mVerificationResultSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return mLastVerified;
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startOnBlankPage();

        mHttpsOrigin = Origin.create("https://www.example.com");
        mHttpOrigin = Origin.create("http://www.android.com");

        mHandleAllUrlsVerifier =
                ChromeOriginVerifierFactory.create(
                        PACKAGE_NAME,
                        CustomTabsService.RELATION_HANDLE_ALL_URLS,
                        new MockWebContents());
        mUseAsOriginVerifier =
                ChromeOriginVerifierFactory.create(
                        PACKAGE_NAME,
                        CustomTabsService.RELATION_USE_AS_ORIGIN,
                        /* webContents= */ null);
        mVerificationResultSemaphore = new Semaphore(0);

        mExternalAuthUtils = new TestExternalAuthUtils();
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtils);
    }

    @Test
    @SmallTest
    public void testOnlyHttpsAllowed() throws InterruptedException {
        Assert.assertFalse(verify(mHandleAllUrlsVerifier, mHttpOrigin));
    }

    @Test
    @SmallTest
    public void testMultipleRelationships() throws Exception {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () ->
                        ChromeOriginVerifier.addVerificationOverride(
                                PACKAGE_NAME,
                                mHttpsOrigin,
                                CustomTabsService.RELATION_USE_AS_ORIGIN));
        Assert.assertTrue(verify(mUseAsOriginVerifier, mHttpsOrigin));
        Assert.assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                ChromeOriginVerifier.wasPreviouslyVerified(
                                        PACKAGE_NAME,
                                        mHttpsOrigin,
                                        CustomTabsService.RELATION_USE_AS_ORIGIN)));
        Assert.assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mUseAsOriginVerifier.wasPreviouslyVerified(mHttpsOrigin)));

        Assert.assertFalse(
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                ChromeOriginVerifier.wasPreviouslyVerified(
                                        PACKAGE_NAME,
                                        mHttpsOrigin,
                                        CustomTabsService.RELATION_HANDLE_ALL_URLS)));

        Assert.assertEquals(mLastPackageName, PACKAGE_NAME);
        Assert.assertEquals(mLastOrigin, mHttpsOrigin);
    }

    @Test
    @SmallTest
    public void testVerificationBypass() throws InterruptedException {
        ChromeOriginVerifier verifier =
                ChromeOriginVerifierFactory.create(
                        PACKAGE_NAME, CustomTabsService.RELATION_HANDLE_ALL_URLS, null);

        Assert.assertFalse(verify(verifier, mHttpsOrigin));

        // Try again, but this time allowlist the package/origin.
        mExternalAuthUtils.addToAllowlist(PACKAGE_NAME, mHttpsOrigin);

        Assert.assertTrue(verify(verifier, mHttpsOrigin));
    }

    @Test
    @SmallTest
    public void testRelationToRelationship() throws InterruptedException {
        Assert.assertEquals(
                "delegate_permission/common.use_as_origin",
                ChromeOriginVerifier.relationToRelationship(
                        CustomTabsService.RELATION_USE_AS_ORIGIN));
        Assert.assertEquals(
                "delegate_permission/common.handle_all_urls",
                ChromeOriginVerifier.relationToRelationship(
                        CustomTabsService.RELATION_HANDLE_ALL_URLS));
    }

    @Test
    @SmallTest
    public void testIsAllowlisted() throws InterruptedException {
        ChromeOriginVerifier verifier =
                ChromeOriginVerifierFactory.create(
                        PACKAGE_NAME, CustomTabsService.RELATION_HANDLE_ALL_URLS, null);
        Assert.assertFalse(
                verifier.isAllowlisted(
                        "no.existing.package",
                        Origin.create("https://not.exist.com"),
                        "delegate_permission/common.handle_all_urls"));
        Assert.assertFalse(
                verifier.isAllowlisted(
                        PACKAGE_NAME, mHttpsOrigin, "delegate_permission/common.handle_all_urls"));
        mExternalAuthUtils.addToAllowlist(PACKAGE_NAME, mHttpsOrigin);
        Assert.assertFalse(
                verifier.isAllowlisted(
                        PACKAGE_NAME, mHttpsOrigin, "delegate_permission/common.use_as_origin"));
        Assert.assertTrue(
                verifier.isAllowlisted(
                        PACKAGE_NAME, mHttpsOrigin, "delegate_permission/common.handle_all_urls"));
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({
        ChromeSwitches.DISABLE_DIGITAL_ASSET_LINK_VERIFICATION
                + "=https://www.example.com;https://not.exist.com;http://www.android.com"
    })
    public void testVerificationSkipByCommandline() throws InterruptedException {
        ChromeOriginVerifier verifier =
                ChromeOriginVerifierFactory.create(
                        PACKAGE_NAME, CustomTabsService.RELATION_HANDLE_ALL_URLS, null);

        Assert.assertTrue(verify(verifier, mHttpsOrigin));
        Assert.assertTrue(verify(verifier, mHttpOrigin));

        Origin origin = Origin.create("https://doesnot.exist.com");

        Assert.assertFalse(verify(verifier, origin));

        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () ->
                        ChromeOriginVerifier.addVerificationOverride(
                                PACKAGE_NAME, origin, CustomTabsService.RELATION_HANDLE_ALL_URLS));

        Assert.assertTrue(verify(verifier, origin));
    }
}
