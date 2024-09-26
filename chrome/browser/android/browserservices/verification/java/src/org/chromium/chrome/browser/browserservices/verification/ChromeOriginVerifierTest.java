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
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
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
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final long TIMEOUT_MS = 1000;

    private static final String PACKAGE_NAME =
            ContextUtils.getApplicationContext().getPackageName();

    private final ChromeOriginVerifierFactory mFactory = new ChromeOriginVerifierFactoryImpl();

    private Origin mHttpsOrigin;
    private Origin mHttpOrigin;
    private TestExternalAuthUtils mExternalAuthUtils;

    private static class TestExternalAuthUtils extends ExternalAuthUtils {
        private List<Pair<String, Origin>> mAllowlist = new ArrayList<>();

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

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        mHttpsOrigin = Origin.create("https://www.example.com");
        mHttpOrigin = Origin.create("http://www.android.com");

        mHandleAllUrlsVerifier =
                mFactory.create(
                        PACKAGE_NAME,
                        CustomTabsService.RELATION_HANDLE_ALL_URLS,
                        new MockWebContents(),
                        null);
        mUseAsOriginVerifier =
                mFactory.create(
                        PACKAGE_NAME,
                        CustomTabsService.RELATION_USE_AS_ORIGIN,
                        /* webContents= */ null,
                        null);
        mVerificationResultSemaphore = new Semaphore(0);

        mExternalAuthUtils = new TestExternalAuthUtils();
    }

    @Test
    @SmallTest
    public void testOnlyHttpsAllowed() throws InterruptedException {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () ->
                        mHandleAllUrlsVerifier.start(
                                new TestOriginVerificationListener(), mHttpOrigin));
        Assert.assertTrue(
                mVerificationResultSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertFalse(mLastVerified);
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
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () ->
                        mUseAsOriginVerifier.start(
                                new TestOriginVerificationListener(), mHttpsOrigin));
        Assert.assertTrue(
                mVerificationResultSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertTrue(mLastVerified);
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
                mFactory.create(
                        PACKAGE_NAME,
                        CustomTabsService.RELATION_HANDLE_ALL_URLS,
                        null,
                        mExternalAuthUtils);

        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> verifier.start(new TestOriginVerificationListener(), mHttpsOrigin));
        Assert.assertTrue(
                mVerificationResultSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertFalse(mLastVerified);

        // Try again, but this time allowlist the package/origin.
        mExternalAuthUtils.addToAllowlist(PACKAGE_NAME, mHttpsOrigin);
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> verifier.start(new TestOriginVerificationListener(), mHttpsOrigin));
        Assert.assertTrue(
                mVerificationResultSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));

        Assert.assertTrue(mLastVerified);
    }

    @Test
    @SmallTest
    public void testRelationToRelationship() throws InterruptedException {
        Assert.assertEquals(
                ChromeOriginVerifier.relationToRelationship(
                        CustomTabsService.RELATION_USE_AS_ORIGIN),
                "delegate_permission/common.use_as_origin");
        Assert.assertEquals(
                ChromeOriginVerifier.relationToRelationship(
                        CustomTabsService.RELATION_HANDLE_ALL_URLS),
                "delegate_permission/common.handle_all_urls");
    }

    @Test
    @SmallTest
    public void testIsAllowlisted() throws InterruptedException {
        ChromeOriginVerifier verifier =
                mFactory.create(
                        PACKAGE_NAME,
                        CustomTabsService.RELATION_HANDLE_ALL_URLS,
                        null,
                        mExternalAuthUtils);
        Assert.assertFalse(
                verifier.isAllowlisted(
                        "no.existing.package",
                        Origin.create("https://not.exist.com"),
                        "delegate_permission/common.handle_all_urls"));
        mExternalAuthUtils.addToAllowlist(PACKAGE_NAME, mHttpsOrigin);
        Assert.assertFalse(
                verifier.isAllowlisted(
                        PACKAGE_NAME, mHttpsOrigin, "delegate_permission/common.use_as_origin"));
        Assert.assertTrue(
                verifier.isAllowlisted(
                        PACKAGE_NAME, mHttpsOrigin, "delegate_permission/common.handle_all_urls"));

        ChromeOriginVerifier verifierNoAuth =
                mFactory.create(
                        PACKAGE_NAME, CustomTabsService.RELATION_HANDLE_ALL_URLS, null, null);
        Assert.assertFalse(
                verifierNoAuth.isAllowlisted(
                        PACKAGE_NAME, mHttpsOrigin, "delegate_permission/common.handle_all_urls"));
    }
}
