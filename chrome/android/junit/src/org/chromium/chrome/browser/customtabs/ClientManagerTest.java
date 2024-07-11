// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static android.os.Looper.getMainLooper;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifierUnitTestSupport.addVerification;

import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.os.Process;

import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.PostMessageServiceConnection;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.IntentUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifierFactoryImpl;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifierJni;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.content_relationship_verification.OriginVerifier;
import org.chromium.components.content_relationship_verification.OriginVerifierJni;
import org.chromium.components.content_relationship_verification.OriginVerifierUnitTestSupport;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.ShadowUrlUtilities;

/** Tests for ClientManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(
        manifest = Config.NONE,
        shadows = {
            ShadowUrlUtilities.class,
            ShadowPackageManager.class,
            ClientManagerTest.ShadowSysUtils.class
        })
public class ClientManagerTest {
    @Implements(SysUtils.class)
    static class ShadowSysUtils {
        public static boolean sIsLowMemory;

        @Implementation
        public static boolean isCurrentlyLowMemory() {
            return sIsLowMemory;
        }
    }

    private static final String URL = "https://www.android.com";
    private static final String PACKAGE_NAME = "org.chromium.chrome";

    private ClientManager mClientManager;
    private CustomTabsSessionToken mSession =
            CustomTabsSessionToken.createMockSessionTokenForTesting();
    private int mUid = Process.myUid();

    @Mock private ClientManager.InstalledAppProviderWrapper mInstalledAppProviderWrapper;

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private OriginVerifier.Natives mMockOriginVerifierJni;

    @Mock private ChromeOriginVerifier.Natives mMockChromeOriginVerifierJni;

    @Mock private Profile mProfile;

    @Mock private ChromeBrowserInitializer mChromeBrowserInitializer;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(OriginVerifierJni.TEST_HOOKS, mMockOriginVerifierJni);

        mJniMocker.mock(ChromeOriginVerifierJni.TEST_HOOKS, mMockChromeOriginVerifierJni);
        Mockito.doAnswer(
                        args -> {
                            return 100L;
                        })
                .when(mMockChromeOriginVerifierJni)
                .init(Mockito.any(), Mockito.any());

        Mockito.doAnswer(
                        args -> {
                            ((Runnable) args.getArgument(0)).run();
                            return null;
                        })
                .when(mChromeBrowserInitializer)
                .runNowOrAfterFullBrowserStarted(Mockito.any());

        ProfileManager.setLastUsedProfileForTesting(mProfile);

        RequestThrottler.purgeAllEntriesForTesting();

        OriginVerifierUnitTestSupport.registerPackageWithSignature(
                shadowOf(ApplicationProvider.getApplicationContext().getPackageManager()),
                PACKAGE_NAME,
                mUid);

        mClientManager =
                new ClientManager(
                        new ChromeOriginVerifierFactoryImpl(),
                        mInstalledAppProviderWrapper,
                        mChromeBrowserInitializer);

        ChromeOriginVerifier.clearCachedVerificationsForTesting();
        UmaRecorderHolder.resetForTesting();

        ShadowUrlUtilities.setTestImpl(
                new ShadowUrlUtilities.TestImpl() {
                    @Override
                    public boolean urlsMatchIgnoringFragments(String url1, String url2) {
                        // Limited implementation that is good enough for these tests.
                        int index1 = url1.indexOf('#');
                        int index2 = url2.indexOf('#');

                        if (index1 != -1) url1 = url1.substring(0, index1);
                        if (index2 != -1) url2 = url2.substring(0, index2);

                        return url1.equals(url2);
                    }
                });
    }

    @Test
    @SmallTest
    public void testNoSessionNoWarmup() {
        Assert.assertEquals(
                ClientManager.CalledWarmup.NO_SESSION_NO_WARMUP,
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
        Assert.assertEquals(
                ClientManager.CalledWarmup.NO_SESSION_NO_WARMUP,
                mClientManager.getWarmupState(mSession));
    }

    @Test
    @SmallTest
    public void testInvalidSessionWarmup() {
        mClientManager.recordUidHasCalledWarmup(mUid);
        Assert.assertEquals(
                ClientManager.CalledWarmup.NO_SESSION_WARMUP,
                mClientManager.getWarmupState(mSession));
    }

    @Test
    @SmallTest
    public void testValidSessionNoWarmup() {
        mClientManager.newSession(mSession, mUid, null, null, null, null);
        Assert.assertEquals(
                ClientManager.CalledWarmup.SESSION_NO_WARMUP_NOT_CALLED,
                mClientManager.getWarmupState(mSession));
    }

    @Test
    @SmallTest
    public void testValidSessionOtherWarmup() {
        mClientManager.recordUidHasCalledWarmup(mUid + 1);
        mClientManager.newSession(mSession, mUid, null, null, null, null);
        Assert.assertEquals(
                ClientManager.CalledWarmup.SESSION_NO_WARMUP_ALREADY_CALLED,
                mClientManager.getWarmupState(mSession));
    }

    @Test
    @SmallTest
    public void testValidSessionWarmup() {
        mClientManager.recordUidHasCalledWarmup(mUid);
        mClientManager.newSession(mSession, mUid, null, null, null, null);
        Assert.assertEquals(
                ClientManager.CalledWarmup.SESSION_WARMUP, mClientManager.getWarmupState(mSession));
    }

    @Test
    @SmallTest
    public void testValidSessionWarmupSeveralCalls() {
        mClientManager.recordUidHasCalledWarmup(mUid);
        mClientManager.newSession(mSession, mUid, null, null, null, null);
        Assert.assertEquals(
                ClientManager.CalledWarmup.SESSION_WARMUP, mClientManager.getWarmupState(mSession));

        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        mClientManager.newSession(token, mUid, null, null, null, null);
        Assert.assertEquals(
                ClientManager.CalledWarmup.SESSION_WARMUP, mClientManager.getWarmupState(token));
    }

    @Test
    @SmallTest
    public void testPredictionOutcomeSuccess() {
        Assert.assertTrue(mClientManager.newSession(mSession, mUid, null, null, null, null));
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        Assert.assertEquals(
                ClientManager.PredictionStatus.GOOD,
                mClientManager.getPredictionOutcome(mSession, URL));
    }

    @Test
    @SmallTest
    public void testPredictionOutcomeNoPrediction() {
        Assert.assertTrue(mClientManager.newSession(mSession, mUid, null, null, null, null));
        mClientManager.recordUidHasCalledWarmup(mUid);
        Assert.assertEquals(
                ClientManager.PredictionStatus.NONE,
                mClientManager.getPredictionOutcome(mSession, URL));
    }

    @Test
    @SmallTest
    public void testPredictionOutcomeBadPrediction() {
        Assert.assertTrue(mClientManager.newSession(mSession, mUid, null, null, null, null));
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        Assert.assertEquals(
                ClientManager.PredictionStatus.BAD,
                mClientManager.getPredictionOutcome(mSession, URL + "#fragment"));
    }

    @Test
    @SmallTest
    public void testPredictionOutcomeIgnoreFragment() {
        Assert.assertTrue(mClientManager.newSession(mSession, mUid, null, null, null, null));
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        mClientManager.setIgnoreFragmentsForSession(mSession, true);
        Assert.assertEquals(
                ClientManager.PredictionStatus.GOOD,
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
        Assert.assertTrue(
                cm.newSession(
                        mSession,
                        mUid,
                        null,
                        MockPostMessageHandler.create(),
                        serviceConnection,
                        null));
        // Should always start with no origin.
        Assert.assertNull(cm.getPostMessageOriginForSessionForTesting(mSession));

        // With no prepopulated origins, this verification should fail.
        cm.verifyAndInitializeWithPostMessageOriginForSession(
                mSession, Origin.create(URL), null, CustomTabsService.RELATION_USE_AS_ORIGIN);
        shadowOf(getMainLooper()).idle();
        Assert.assertNull(cm.getPostMessageOriginForSessionForTesting(mSession));

        // If there is a prepopulated origin, we should get a synchronous verification.
        addVerification(PACKAGE_NAME, Origin.create(URL), CustomTabsService.RELATION_USE_AS_ORIGIN);
        cm.verifyAndInitializeWithPostMessageOriginForSession(
                mSession, Origin.create(URL), null, CustomTabsService.RELATION_USE_AS_ORIGIN);
        shadowOf(getMainLooper()).idle();

        Assert.assertNotNull(cm.getPostMessageOriginForSessionForTesting(mSession));

        Uri verifiedOrigin = cm.getPostMessageOriginForSessionForTesting(mSession);
        Assert.assertEquals(IntentUtils.ANDROID_APP_REFERRER_SCHEME, verifiedOrigin.getScheme());

        // initializeWithPostMessageOriginForSession should override without checking
        // origin.
        cm.initializeWithPostMessageOriginForSession(mSession, null, null);
        Assert.assertNull(cm.getPostMessageOriginForSessionForTesting(mSession));
        Assert.assertNull(cm.getPostMessageTargetOriginForSessionForTesting(mSession));
    }

    @Test
    @SmallTest
    public void testPostMessageOriginDifferentRelations() {
        final ClientManager cm = mClientManager;
        PostMessageServiceConnection serviceConnection =
                new PostMessageServiceConnection(mSession) {};
        Assert.assertTrue(
                cm.newSession(
                        mSession,
                        mUid,
                        null,
                        MockPostMessageHandler.create(),
                        serviceConnection,
                        null));

        Origin origin = Origin.create(URL);
        when(mInstalledAppProviderWrapper.isAppInstalledAndAssociatedWithOrigin(any(), eq(origin)))
                .thenReturn(true);

        // Should always start with no origin.
        Assert.assertNull(cm.getPostMessageOriginForSessionForTesting(mSession));

        // With no prepopulated origins, this verification should fail.
        cm.verifyAndInitializeWithPostMessageOriginForSession(
                mSession, origin, null, CustomTabsService.RELATION_USE_AS_ORIGIN);
        Assert.assertNull(cm.getPostMessageOriginForSessionForTesting(mSession));

        // Prepopulated origins should depend on the relation used.
        addVerification(PACKAGE_NAME, origin, CustomTabsService.RELATION_HANDLE_ALL_URLS);
        // This uses CustomTabsService.RELATION_USE_AS_ORIGIN by default.
        Assert.assertFalse(cm.isFirstPartyOriginForSession(mSession, origin));

        cm.verifyAndInitializeWithPostMessageOriginForSession(
                mSession, origin, null, CustomTabsService.RELATION_HANDLE_ALL_URLS);

        //        ThreadUtils.runOnUiThreadBlocking(() -> {
        Uri verifiedOrigin = cm.getPostMessageOriginForSessionForTesting(mSession);
        Assert.assertEquals(IntentUtils.ANDROID_APP_REFERRER_SCHEME, verifiedOrigin.getScheme());
        // initializeWithPostMessageOriginForSession should override without checking
        // origin.
        cm.initializeWithPostMessageOriginForSession(mSession, null, null);
        Assert.assertNull(cm.getPostMessageOriginForSessionForTesting(mSession));
        //        });
    }

    @Test
    @SmallTest
    public void testFirstLowConfidencePredictionIsNotThrottled() {
        Assert.assertTrue(mClientManager.newSession(mSession, mUid, null, null, null, null));

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
        String name = "CustomTabs.MayLaunchUrlType";
        Assert.assertTrue(mClientManager.newSession(mSession, mUid, null, null, null, null));

        // No prediction;
        mClientManager.registerLaunch(mSession, URL);
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        name, ClientManager.MayLaunchUrlType.NO_MAY_LAUNCH_URL));

        // Low confidence.
        RequestThrottler.purgeAllEntriesForTesting();
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, null, true));
        mClientManager.registerLaunch(mSession, URL);
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        name, ClientManager.MayLaunchUrlType.LOW_CONFIDENCE));

        // High confidence.
        RequestThrottler.purgeAllEntriesForTesting();
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        mClientManager.registerLaunch(mSession, URL);
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        name, ClientManager.MayLaunchUrlType.HIGH_CONFIDENCE));

        // Low and High confidence.
        RequestThrottler.purgeAllEntriesForTesting();
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, null, true));
        mClientManager.registerLaunch(mSession, URL);
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        name, ClientManager.MayLaunchUrlType.BOTH));

        // Low and High confidence, same call.
        RequestThrottler.purgeAllEntriesForTesting();
        UmaRecorderHolder.resetForTesting();
        Assert.assertTrue(
                mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, true));
        mClientManager.registerLaunch(mSession, URL);
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        name, ClientManager.MayLaunchUrlType.BOTH));
    }

    @Test
    @SmallTest
    public void testPostMessageWithTargetOrigin() {
        final ClientManager cm = mClientManager;

        PostMessageServiceConnection serviceConnection =
                new PostMessageServiceConnection(mSession) {};
        Assert.assertTrue(
                cm.newSession(
                        mSession,
                        mUid,
                        null,
                        MockPostMessageHandler.create(),
                        serviceConnection,
                        null));
        // Should always start with no origin.
        Assert.assertNull(cm.getPostMessageOriginForSessionForTesting(mSession));
        Assert.assertNull(cm.getPostMessageTargetOriginForSessionForTesting(mSession));

        // If there is a prepopulated origin, we should get a synchronous verification.
        addVerification(PACKAGE_NAME, Origin.create(URL), CustomTabsService.RELATION_USE_AS_ORIGIN);
        cm.verifyAndInitializeWithPostMessageOriginForSession(
                mSession,
                Origin.create(URL),
                Origin.create(URL),
                CustomTabsService.RELATION_USE_AS_ORIGIN);
        shadowOf(getMainLooper()).idle();

        Assert.assertEquals(
                URL, cm.getPostMessageTargetOriginForSessionForTesting(mSession).toString());

        // initializeWithPostMessageOriginForSession should override without checking
        // origin.
        cm.initializeWithPostMessageOriginForSession(mSession, null, null);
        Assert.assertNull(cm.getPostMessageOriginForSessionForTesting(mSession));
        Assert.assertNull(cm.getPostMessageTargetOriginForSessionForTesting(mSession));
    }

    @Test
    @SmallTest
    public void testLogConnectionClosedCTForeground() {
        String histogramName = "CustomTabs.SessionDisconnectStatus";
        ShadowSysUtils.sIsLowMemory = false;

        Assert.assertTrue(
                "A new session should have been created.",
                mClientManager.newSession(mSession, mUid, null, null, null, null));
        mClientManager.setCustomTabIsInForeground(mSession, true);
        mClientManager.dontKeepAliveForSession(mSession);

        mClientManager.cleanupSession(mSession);

        Assert.assertEquals(
                "Only one histogram value should have been logged",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, ClientManager.SessionDisconnectStatus.CT_FOREGROUND));
    }

    @Test
    @SmallTest
    public void testLogConnectionClosedCTForegroundKeepAlive() {
        String histogramName = "CustomTabs.SessionDisconnectStatus";
        ShadowSysUtils.sIsLowMemory = false;
        Intent intent =
                new Intent()
                        .setComponent(
                                new ComponentName(
                                        ApplicationProvider.getApplicationContext(),
                                        ChromeLauncherActivity.class));

        Assert.assertTrue(
                "A new session should have been created.",
                mClientManager.newSession(mSession, mUid, null, null, null, null));
        mClientManager.setCustomTabIsInForeground(mSession, true);
        mClientManager.keepAliveForSession(mSession, intent);

        mClientManager.cleanupSession(mSession);

        Assert.assertEquals(
                "Only one histogram value should have been logged",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName,
                        ClientManager.SessionDisconnectStatus.CT_FOREGROUND_KEEP_ALIVE));
    }

    @Test
    @SmallTest
    public void testLogConnectionClosedCTBackground() {
        String histogramName = "CustomTabs.SessionDisconnectStatus";
        ShadowSysUtils.sIsLowMemory = false;

        Assert.assertTrue(
                "A new session should have been created.",
                mClientManager.newSession(mSession, mUid, null, null, null, null));
        mClientManager.setCustomTabIsInForeground(mSession, false);
        mClientManager.dontKeepAliveForSession(mSession);

        mClientManager.cleanupSession(mSession);

        Assert.assertEquals(
                "Only one histogram value should have been logged",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, ClientManager.SessionDisconnectStatus.CT_BACKGROUND));
    }

    @Test
    @SmallTest
    public void testLogConnectionClosedCTBackgroundKeepAlive() {
        String histogramName = "CustomTabs.SessionDisconnectStatus";
        ShadowSysUtils.sIsLowMemory = false;
        Intent intent =
                new Intent()
                        .setComponent(
                                new ComponentName(
                                        ApplicationProvider.getApplicationContext(),
                                        ChromeLauncherActivity.class));

        Assert.assertTrue(
                "A new session should have been created.",
                mClientManager.newSession(mSession, mUid, null, null, null, null));
        mClientManager.setCustomTabIsInForeground(mSession, false);
        mClientManager.keepAliveForSession(mSession, intent);

        mClientManager.cleanupSession(mSession);

        Assert.assertEquals(
                "Only one histogram value should have been logged",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName,
                        ClientManager.SessionDisconnectStatus.CT_BACKGROUND_KEEP_ALIVE));
    }

    @Test
    @SmallTest
    public void testLogConnectionClosedLowMemoryCTForeground() {
        String histogramName = "CustomTabs.SessionDisconnectStatus";
        ShadowSysUtils.sIsLowMemory = true;

        Assert.assertTrue(
                "A new session should have been created.",
                mClientManager.newSession(mSession, mUid, null, null, null, null));
        mClientManager.setCustomTabIsInForeground(mSession, true);
        mClientManager.dontKeepAliveForSession(mSession);

        mClientManager.cleanupSession(mSession);

        Assert.assertEquals(
                "Only one histogram value should have been logged",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName,
                        ClientManager.SessionDisconnectStatus.LOW_MEMORY_CT_FOREGROUND));
    }

    @Test
    @SmallTest
    public void testLogConnectionClosedLowMemoryCTForegroundKeepAlive() {
        String histogramName = "CustomTabs.SessionDisconnectStatus";
        ShadowSysUtils.sIsLowMemory = true;
        Intent intent =
                new Intent()
                        .setComponent(
                                new ComponentName(
                                        ApplicationProvider.getApplicationContext(),
                                        ChromeLauncherActivity.class));

        Assert.assertTrue(
                "A new session should have been created.",
                mClientManager.newSession(mSession, mUid, null, null, null, null));
        mClientManager.setCustomTabIsInForeground(mSession, true);
        mClientManager.keepAliveForSession(mSession, intent);

        mClientManager.cleanupSession(mSession);

        Assert.assertEquals(
                "Only one histogram value should have been logged",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName,
                        ClientManager.SessionDisconnectStatus.LOW_MEMORY_CT_FOREGROUND_KEEP_ALIVE));
    }

    @Test
    @SmallTest
    public void testLogConnectionClosedLowMemoryCTBackground() {
        String histogramName = "CustomTabs.SessionDisconnectStatus";
        ShadowSysUtils.sIsLowMemory = true;

        Assert.assertTrue(
                "A new session should have been created.",
                mClientManager.newSession(mSession, mUid, null, null, null, null));
        mClientManager.setCustomTabIsInForeground(mSession, false);
        mClientManager.dontKeepAliveForSession(mSession);

        mClientManager.cleanupSession(mSession);

        Assert.assertEquals(
                "Only one histogram value should have been logged",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName,
                        ClientManager.SessionDisconnectStatus.LOW_MEMORY_CT_BACKGROUND));
    }

    @Test
    @SmallTest
    public void testLogConnectionClosedLowMemoryCTBackgroundKeepAlive() {
        String histogramName = "CustomTabs.SessionDisconnectStatus";
        ShadowSysUtils.sIsLowMemory = true;
        Intent intent =
                new Intent()
                        .setComponent(
                                new ComponentName(
                                        ApplicationProvider.getApplicationContext(),
                                        ChromeLauncherActivity.class));

        Assert.assertTrue(
                "A new session should have been created.",
                mClientManager.newSession(mSession, mUid, null, null, null, null));
        mClientManager.setCustomTabIsInForeground(mSession, false);
        mClientManager.keepAliveForSession(mSession, intent);

        mClientManager.cleanupSession(mSession);

        Assert.assertEquals(
                "Only one histogram value should have been logged",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName,
                        ClientManager.SessionDisconnectStatus.LOW_MEMORY_CT_BACKGROUND_KEEP_ALIVE));
    }

    @Test
    @SmallTest
    public void testLogConnectionClosedCleanupCalledTwiceLogsOnce() {
        String histogramName = "CustomTabs.SessionDisconnectStatus";
        ShadowSysUtils.sIsLowMemory = false;

        Assert.assertTrue(
                "A new session should have been created.",
                mClientManager.newSession(mSession, mUid, null, null, null, null));
        mClientManager.setCustomTabIsInForeground(mSession, true);
        mClientManager.dontKeepAliveForSession(mSession);

        mClientManager.cleanupSession(mSession);
        mClientManager.cleanupSession(mSession);

        Assert.assertEquals(
                "Only one histogram value should have been logged",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, ClientManager.SessionDisconnectStatus.CT_FOREGROUND));
    }
}
