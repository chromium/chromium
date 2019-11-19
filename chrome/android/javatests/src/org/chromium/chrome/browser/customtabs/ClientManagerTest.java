// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.net.Uri;
import android.os.Process;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.MetricsUtils;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.OriginVerifier;
import org.chromium.chrome.browser.browserservices.PostMessageHandler;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.PostMessageServiceConnection;

/** Tests for ClientManager. */
@RunWith(BaseJUnit4ClassRunner.class)
public class ClientManagerTest {
    @Rule
    public NativeLibraryTestRule mActivityTestRule = new NativeLibraryTestRule();

    private static final String URL = "https://www.android.com";
    private static final String HTTP_URL = "http://www.android.com";

    private ClientManager mClientManager;
    private CustomTabsSessionToken mSession =
            CustomTabsSessionToken.createMockSessionTokenForTesting();
    private int mUid = Process.myUid();

    @Before
    public void setUp() {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        mActivityTestRule.loadNativeLibraryNoBrowserProcess();
        RequestThrottler.purgeAllEntriesForTesting();
        mClientManager = new ClientManager();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> OriginVerifier.clearCachedVerificationsForTesting());
    }

    @Test
    @SmallTest
    public void testNoSessionNoWarmup() {
        Assert.assertEquals(ClientManager.CalledWarmup.NO_SESSION_NO_WARMUP,
                mClientManager.getWarmupState(null));
    }

    @Test
    @SmallTest
    public void testNoSessionWarmup() {
        mClientManager.recordUidHasCalledWarmup(mUid);
        Assert.assertEquals(
                ClientManager.CalledWarmup.NO_SESSION_WARMUP, mClientManager.getWarmupState(null));
    }

    @Test
    @SmallTest
    public void testInvalidSessionNoWarmup() {
        Assert.assertEquals(ClientManager.CalledWarmup.NO_SESSION_NO_WARMUP,
                mClientManager.getWarmupState(mSession));
    }

    @Test
    @SmallTest
    public void testInvalidSessionWarmup() {
        mClientManager.recordUidHasCalledWarmup(mUid);
        Assert.assertEquals(ClientManager.CalledWarmup.NO_SESSION_WARMUP,
                mClientManager.getWarmupState(mSession));
    }

    @Test
    @SmallTest
    public void testValidSessionNoWarmup() {
        mClientManager.newSession(mSession, mUid, null, null, null);
        Assert.assertEquals(ClientManager.CalledWarmup.SESSION_NO_WARMUP_NOT_CALLED,
                mClientManager.getWarmupState(mSession));
    }

    @Test
    @SmallTest
    public void testValidSessionOtherWarmup() {
        mClientManager.recordUidHasCalledWarmup(mUid + 1);
        mClientManager.newSession(mSession, mUid, null, null, null);
        Assert.assertEquals(ClientManager.CalledWarmup.SESSION_NO_WARMUP_ALREADY_CALLED,
                mClientManager.getWarmupState(mSession));
    }

    @Test
    @SmallTest
    public void testValidSessionWarmup() {
        mClientManager.recordUidHasCalledWarmup(mUid);
        mClientManager.newSession(mSession, mUid, null, null, null);
        Assert.assertEquals(
                ClientManager.CalledWarmup.SESSION_WARMUP, mClientManager.getWarmupState(mSession));
    }

    @Test
    @SmallTest
    public void testValidSessionWarmupSeveralCalls() {
        mClientManager.recordUidHasCalledWarmup(mUid);
        mClientManager.newSession(mSession, mUid, null, null, null);
        Assert.assertEquals(
                ClientManager.CalledWarmup.SESSION_WARMUP, mClientManager.getWarmupState(mSession));

        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        mClientManager.newSession(token, mUid, null, null, null);
        Assert.assertEquals(
                ClientManager.CalledWarmup.SESSION_WARMUP, mClientManager.getWarmupState(token));
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testPredictionOutcomeSuccess() {
        Assert.assertTrue(mClientManager.newSession(mSession, mUid, null, null, null));
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        Assert.assertEquals(ClientManager.PredictionStatus.GOOD,
                mClientManager.getPredictionOutcome(mSession, URL));
    }

    @Test
    @SmallTest
    public void testPredictionOutcomeNoPrediction() {
        Assert.assertTrue(mClientManager.newSession(mSession, mUid, null, null, null));
        mClientManager.recordUidHasCalledWarmup(mUid);
        Assert.assertEquals(ClientManager.PredictionStatus.NONE,
                mClientManager.getPredictionOutcome(mSession, URL));
    }

    @Test
    @SmallTest
    public void testPredictionOutcomeBadPrediction() {
        Assert.assertTrue(mClientManager.newSession(mSession, mUid, null, null, null));
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        Assert.assertEquals(ClientManager.PredictionStatus.BAD,
                mClientManager.getPredictionOutcome(mSession, URL + "#fragment"));
    }

    @Test
    @SmallTest
    public void testPredictionOutcomeIgnoreFragment() {
        Assert.assertTrue(mClientManager.newSession(mSession, mUid, null, null, null));
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        mClientManager.setIgnoreFragmentsForSession(mSession, true);
        Assert.assertEquals(ClientManager.PredictionStatus.GOOD,
                mClientManager.getPredictionOutcome(mSession, URL + "#fragment"));
    }

    @Test
    @SmallTest
    public void testPostMessageOriginVerification() {
        final ClientManager cm = mClientManager;
        // TODO(peconn): Get rid of this anonymous class once PostMessageServiceConnection is made
        // non-abstract. Same with the other occurrences below.
        PostMessageServiceConnection serviceConnection =
                new PostMessageServiceConnection(mSession) {};
        Assert.assertTrue(cm.newSession(mSession, mUid, null,
                new PostMessageHandler(serviceConnection), serviceConnection));
        // Should always start with no origin.
        Assert.assertNull(cm.getPostMessageOriginForSessionForTesting(mSession));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // With no prepopulated origins, this verification should fail.
            cm.verifyAndInitializeWithPostMessageOriginForSession(
                    mSession, Origin.create(URL), CustomTabsService.RELATION_USE_AS_ORIGIN);
            Assert.assertNull(cm.getPostMessageOriginForSessionForTesting(mSession));

            // If there is a prepopulated origin, we should get a synchronous verification.
            OriginVerifier.addVerificationOverride(
                    ContextUtils.getApplicationContext().getPackageName(), Origin.create(URL),
                    CustomTabsService.RELATION_USE_AS_ORIGIN);
            cm.verifyAndInitializeWithPostMessageOriginForSession(
                    mSession, Origin.create(URL), CustomTabsService.RELATION_USE_AS_ORIGIN);
        });

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return cm.getPostMessageOriginForSessionForTesting(mSession) != null;
            }
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Uri verifiedOrigin = cm.getPostMessageOriginForSessionForTesting(mSession);
            Assert.assertEquals(
                    IntentHandler.ANDROID_APP_REFERRER_SCHEME, verifiedOrigin.getScheme());

            // initializeWithPostMessageOriginForSession should override without checking
            // origin.
            cm.initializeWithPostMessageOriginForSession(mSession, null);
            Assert.assertNull(cm.getPostMessageOriginForSessionForTesting(mSession));
        });
    }

    @Test
    @SmallTest
    public void testPostMessageOriginDifferentRelations() {
        final ClientManager cm = mClientManager;
        PostMessageServiceConnection serviceConnection =
                new PostMessageServiceConnection(mSession) {};
        Assert.assertTrue(cm.newSession(mSession, mUid, null,
                new PostMessageHandler(serviceConnection), serviceConnection));
        // Should always start with no origin.
        Assert.assertNull(cm.getPostMessageOriginForSessionForTesting(mSession));

        Origin origin = Origin.create(URL);

        // With no prepopulated origins, this verification should fail.
        cm.verifyAndInitializeWithPostMessageOriginForSession(
                mSession, origin, CustomTabsService.RELATION_USE_AS_ORIGIN);
        Assert.assertNull(cm.getPostMessageOriginForSessionForTesting(mSession));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Prepopulated origins should depend on the relation used.
            OriginVerifier.addVerificationOverride(
                    ContextUtils.getApplicationContext().getPackageName(), origin,
                    CustomTabsService.RELATION_HANDLE_ALL_URLS);
            // This uses CustomTabsService.RELATION_USE_AS_ORIGIN by default.
            Assert.assertFalse(cm.isFirstPartyOriginForSession(mSession, origin));
        });

        cm.verifyAndInitializeWithPostMessageOriginForSession(
                mSession, origin, CustomTabsService.RELATION_HANDLE_ALL_URLS);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Uri verifiedOrigin = cm.getPostMessageOriginForSessionForTesting(mSession);
            Assert.assertEquals(
                    IntentHandler.ANDROID_APP_REFERRER_SCHEME, verifiedOrigin.getScheme());
            // initializeWithPostMessageOriginForSession should override without checking
            // origin.
            cm.initializeWithPostMessageOriginForSession(mSession, null);
            Assert.assertNull(cm.getPostMessageOriginForSessionForTesting(mSession));
        });
    }

    @Test
    @SmallTest
    public void testPostMessageOriginHttpNotAllowed() {
        final ClientManager cm = mClientManager;
        PostMessageServiceConnection serviceConnection =
                new PostMessageServiceConnection(mSession) {};
        Assert.assertTrue(cm.newSession(mSession, mUid, null,
                new PostMessageHandler(serviceConnection), serviceConnection));
        // Should always start with no origin.
        Assert.assertNull(cm.getPostMessageOriginForSessionForTesting(mSession));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Origin origin = Origin.create(HTTP_URL);
            // With no prepopulated origins, this verification should fail.
            cm.verifyAndInitializeWithPostMessageOriginForSession(
                    mSession, origin, CustomTabsService.RELATION_USE_AS_ORIGIN);
            Assert.assertNull(cm.getPostMessageOriginForSessionForTesting(mSession));

            // Even if there is a prepopulated origin, non-https origins should get an early
            // return with false.
            OriginVerifier.addVerificationOverride(
                    ContextUtils.getApplicationContext().getPackageName(), origin,
                    CustomTabsService.RELATION_USE_AS_ORIGIN);
            cm.verifyAndInitializeWithPostMessageOriginForSession(
                    mSession, origin, CustomTabsService.RELATION_USE_AS_ORIGIN);
            Assert.assertNull(cm.getPostMessageOriginForSessionForTesting(mSession));
        });
    }

    @Test
    @SmallTest
    public void testFirstLowConfidencePredictionIsNotThrottled() {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        Assert.assertTrue(mClientManager.newSession(mSession, mUid, null, null, null));

        // Two low confidence in a row is OK.
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, null, true));
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, null, true));
        mClientManager.registerLaunch(mSession, URL);

        // Low -> High as well.
        RequestThrottler.purgeAllEntriesForTesting();
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, null, true));
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        mClientManager.registerLaunch(mSession, URL);

        // High -> Low as well.
        RequestThrottler.purgeAllEntriesForTesting();
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, null, true));
        mClientManager.registerLaunch(mSession, URL);
    }

    @Test
    @SmallTest
    public void testMayLaunchUrlAccounting() {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();

        String name = "CustomTabs.MayLaunchUrlType";
        MetricsUtils.HistogramDelta noMayLaunchUrlDelta = new MetricsUtils.HistogramDelta(
                name, ClientManager.MayLaunchUrlType.NO_MAY_LAUNCH_URL);
        MetricsUtils.HistogramDelta lowConfidenceDelta = new MetricsUtils.HistogramDelta(
                name, ClientManager.MayLaunchUrlType.LOW_CONFIDENCE);
        MetricsUtils.HistogramDelta highConfidenceDelta = new MetricsUtils.HistogramDelta(
                name, ClientManager.MayLaunchUrlType.HIGH_CONFIDENCE);
        MetricsUtils.HistogramDelta bothDelta =
                new MetricsUtils.HistogramDelta(name, ClientManager.MayLaunchUrlType.BOTH);

        Assert.assertTrue(mClientManager.newSession(mSession, mUid, null, null, null));

        // No prediction;
        mClientManager.registerLaunch(mSession, URL);
        Assert.assertEquals(1, noMayLaunchUrlDelta.getDelta());

        // Low confidence.
        RequestThrottler.purgeAllEntriesForTesting();
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, null, true));
        mClientManager.registerLaunch(mSession, URL);
        Assert.assertEquals(1, lowConfidenceDelta.getDelta());

        // High confidence.
        RequestThrottler.purgeAllEntriesForTesting();
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        mClientManager.registerLaunch(mSession, URL);
        Assert.assertEquals(1, highConfidenceDelta.getDelta());

        // Low and High confidence.
        RequestThrottler.purgeAllEntriesForTesting();
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, null, true));
        mClientManager.registerLaunch(mSession, URL);
        Assert.assertEquals(1, bothDelta.getDelta());

        // Low and High confidence, same call.
        RequestThrottler.purgeAllEntriesForTesting();
        bothDelta = new MetricsUtils.HistogramDelta(name, ClientManager.MayLaunchUrlType.BOTH);
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, true));
        mClientManager.registerLaunch(mSession, URL);
        Assert.assertEquals(1, bothDelta.getDelta());
    }
}
