// Copyright 2015 The Chromium Authors. All rights reserved.
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
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.browserservices.verification.OriginVerifier.OriginVerificationListener;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Tests for OriginVerifier. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(OriginVerifierTest.TEST_BATCH_NAME)
public class OriginVerifierTest {
    public static final String TEST_BATCH_NAME = "origin_verifier";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final long TIMEOUT_MS = 1000;

    private static final String PACKAGE_NAME =
            ContextUtils.getApplicationContext().getPackageName();

    private final OriginVerifierFactory mFactory = new OriginVerifierFactoryImpl();

    private Origin mHttpsOrigin;
    private Origin mHttpOrigin;
    private TestExternalAuthUtils mExternalAuthUtils;

    private class TestExternalAuthUtils extends ExternalAuthUtils {
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
    private OriginVerifier mUseAsOriginVerifier;
    private OriginVerifier mHandleAllUrlsVerifier;
    private volatile String mLastPackageName;
    private volatile Origin mLastOrigin;
    private volatile boolean mLastVerified;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        mHttpsOrigin = Origin.create("https://www.example.com");
        mHttpOrigin = Origin.create("http://www.android.com");

        mHandleAllUrlsVerifier = mFactory.create(PACKAGE_NAME,
                CustomTabsService.RELATION_HANDLE_ALL_URLS, new MockWebContents(), null);
        mUseAsOriginVerifier = mFactory.create(PACKAGE_NAME,
                CustomTabsService.RELATION_USE_AS_ORIGIN, /* webContents= */ null, null);
        mVerificationResultSemaphore = new Semaphore(0);

        mExternalAuthUtils = new TestExternalAuthUtils();
    }

    @Test
    @SmallTest
    public void testOnlyHttpsAllowed() throws InterruptedException {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                ()
                        -> mHandleAllUrlsVerifier.start(
                                new TestOriginVerificationListener(), mHttpOrigin));
        Assert.assertTrue(
                mVerificationResultSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertFalse(mLastVerified);
    }

    @Test
    @SmallTest
    public void testMultipleRelationships() throws Exception {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                ()
                        -> OriginVerifier.addVerificationOverride(PACKAGE_NAME, mHttpsOrigin,
                                CustomTabsService.RELATION_USE_AS_ORIGIN));
        PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                ()
                        -> mUseAsOriginVerifier.start(
                                new TestOriginVerificationListener(), mHttpsOrigin));
        Assert.assertTrue(
                mVerificationResultSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertTrue(mLastVerified);
        Assert.assertTrue(TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> OriginVerifier.wasPreviouslyVerified(PACKAGE_NAME, mHttpsOrigin,
                                CustomTabsService.RELATION_USE_AS_ORIGIN)));
        Assert.assertFalse(TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> OriginVerifier.wasPreviouslyVerified(PACKAGE_NAME, mHttpsOrigin,
                                CustomTabsService.RELATION_HANDLE_ALL_URLS)));
        Assert.assertEquals(mLastPackageName, PACKAGE_NAME);
        Assert.assertEquals(mLastOrigin, mHttpsOrigin);
    }

    @Test
    @SmallTest
    public void testWipedWithBrowsingData() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();

        String relationship = "relationship1";
        Set<String> savedLinks = new HashSet<>();
        savedLinks.add(relationship);

        VerificationResultStore mStore = VerificationResultStore.getInstance();

        mStore.setRelationships(savedLinks);

        Assert.assertTrue(mStore.getRelationships().contains(relationship));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingData(callbackHelper::notifyCalled,
                    new int[] {BrowsingDataType.HISTORY}, TimePeriod.ALL_TIME);
        });

        callbackHelper.waitForCallback(0);
        Assert.assertTrue(mStore.getRelationships().isEmpty());
    }

    @Test
    @SmallTest
    public void testVerificationBypass() throws InterruptedException {
        OriginVerifier verifier = mFactory.create(
                PACKAGE_NAME, CustomTabsService.RELATION_HANDLE_ALL_URLS, null, mExternalAuthUtils);

        PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                () -> verifier.start(new TestOriginVerificationListener(), mHttpsOrigin));
        Assert.assertTrue(
                mVerificationResultSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertFalse(mLastVerified);

        // Try again, but this time allowlist the package/origin.
        mExternalAuthUtils.addToAllowlist(PACKAGE_NAME, mHttpsOrigin);
        PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                () -> verifier.start(new TestOriginVerificationListener(), mHttpsOrigin));
        Assert.assertTrue(
                mVerificationResultSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertTrue(mLastVerified);
    }
}
