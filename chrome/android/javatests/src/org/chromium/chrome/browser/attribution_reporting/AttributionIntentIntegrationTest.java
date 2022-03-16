// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.view.KeyEvent;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.StartupTabPreloader;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.AttributionReporter;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests attribution reporting intents.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class AttributionIntentIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    public EmptyTabObserver mTabObserver;

    @Mock
    public AttributionReporter mAttributionReporter;

    @Captor
    public ArgumentCaptor<NavigationHandle> navigationHandleCaptor;

    private BroadcastReceiver mReceiver;
    private Intent mAttributionIntentReceived;
    private ActivityStateListener mActivityStateListener;
    private ActivityTabTabObserver mActiveTabObserver;

    @Before
    public void setUp() {
        AttributionIntentHandlerFactory.setInputEventValidatorForTesting((inputEvent) -> true);

        // We need to pass Attribution Intents through a BroadcastReceiver if we want to track them
        // as ActivityMonitors don't work for PendingIntents.
        mReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                mAttributionIntentReceived = intent;
                ContextUtils.getApplicationContext().startActivity(intent);
            }
        };
        IntentFilter filter = new IntentFilter(AttributionConstants.ACTION_APP_ATTRIBUTION);
        ContextUtils.getApplicationContext().registerReceiver(mReceiver, filter);

        mActivityStateListener = (activity, newState) -> {
            if (newState == ActivityState.CREATED && activity instanceof ChromeActivity) {
                mActiveTabObserver = new ActivityTabTabObserver(
                        ((ChromeActivity) activity).getActivityTabProvider()) {
                    @Override
                    protected void onObservingDifferentTab(Tab tab, boolean hint) {
                        tab.addObserver(mTabObserver);
                    }
                };
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> ApplicationStatus.registerStateListenerForAllActivities(
                                mActivityStateListener));
    }

    @After
    public void tearDown() {
        AttributionReporter.setInstanceForTesting(null);
        ContextUtils.getApplicationContext().unregisterReceiver(mReceiver);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ApplicationStatus.unregisterActivityStateListener(mActivityStateListener));
    }

    private Intent makeValidAttributionIntent(
            String eventId, String destination, String reportTo, long expiry) {
        Intent innerIntent = new Intent(AttributionConstants.ACTION_APP_ATTRIBUTION);
        innerIntent.putExtra(AttributionConstants.EXTRA_ATTRIBUTION_SOURCE_EVENT_ID, eventId);
        innerIntent.putExtra(AttributionConstants.EXTRA_ATTRIBUTION_DESTINATION, destination);
        innerIntent.putExtra(AttributionConstants.EXTRA_ATTRIBUTION_REPORT_TO, reportTo);
        if (expiry != 0) {
            innerIntent.putExtra(AttributionConstants.EXTRA_ATTRIBUTION_EXPIRY, expiry);
        }
        innerIntent.putExtra(AttributionConstants.EXTRA_INPUT_EVENT,
                new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
        innerIntent.setPackage(ContextUtils.getApplicationContext().getPackageName());
        innerIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Intent outerIntent = new Intent(Intent.ACTION_VIEW);
        PendingIntent pendingIntent =
                PendingIntent.getBroadcast(ContextUtils.getApplicationContext(), 0, innerIntent,
                        IntentUtils.getPendingIntentMutabilityFlag(true)
                                | PendingIntent.FLAG_CANCEL_CURRENT);
        outerIntent.putExtra(AttributionConstants.EXTRA_ATTRIBUTION_INTENT, pendingIntent);
        return outerIntent;
    }

    private void doTestConversionIntentEnabledInner(
            boolean disableStartupTabPreloader, Callback<Intent> startActivityCallback) {
        Intent outerIntent = makeValidAttributionIntent(
                "1234", "https://example.com", "https://example2.com", 5678);
        outerIntent.putExtra(StartupTabPreloader.EXTRA_DISABLE_STARTUP_TAB_PRELOADER,
                disableStartupTabPreloader);
        startActivityCallback.onResult(outerIntent);
        Assert.assertNotNull(mAttributionIntentReceived);
        Assert.assertEquals("1234",
                mAttributionIntentReceived.getStringExtra(
                        AttributionConstants.EXTRA_ATTRIBUTION_SOURCE_EVENT_ID));
        Assert.assertEquals("https://example.com",
                mAttributionIntentReceived.getStringExtra(
                        AttributionConstants.EXTRA_ATTRIBUTION_DESTINATION));
        Assert.assertEquals("https://example2.com",
                mAttributionIntentReceived.getStringExtra(
                        AttributionConstants.EXTRA_ATTRIBUTION_REPORT_TO));
        Assert.assertEquals(5678,
                mAttributionIntentReceived.getLongExtra(
                        AttributionConstants.EXTRA_ATTRIBUTION_EXPIRY, 0));

        Mockito.verify(mTabObserver, Mockito.times(1))
                .onDidFinishNavigation(any(), navigationHandleCaptor.capture());
        NavigationHandle handle = navigationHandleCaptor.getValue();

        Assert.assertEquals("android-app", handle.getInitiatorOrigin().getScheme());
        Assert.assertEquals(ContextUtils.getApplicationContext().getPackageName(),
                handle.getInitiatorOrigin().getHost());

        Assert.assertEquals(1234L, handle.getImpression().impressionData);
        Assert.assertEquals("example.com", handle.getImpression().conversionDestination.host);
        Assert.assertEquals("example2.com", handle.getImpression().reportingOrigin.host);
        Assert.assertEquals(5678 * 1000L, handle.getImpression().expiry.microseconds);
    }

    @Test
    @LargeTest
    @Feature({"ConversionMeasurement"})
    @Features.EnableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    public void testConversionIntentEnabled() {
        doTestConversionIntentEnabledInner(false, (Intent intent) -> {
            mTabbedActivityTestRule.startMainActivityFromIntent(intent, "about:blank");
        });
    }

    @Test
    @LargeTest
    @Feature({"ConversionMeasurement"})
    @Features.EnableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    public void testConversionIntentEnabled_noStartupTabPreloader() {
        doTestConversionIntentEnabledInner(true, (Intent intent) -> {
            mTabbedActivityTestRule.startMainActivityFromIntent(intent, "about:blank");
        });
    }

    @Test
    @LargeTest
    @Feature({"ConversionMeasurement"})
    @Features.EnableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    public void testConversionIntentEnabled_CCT() {
        doTestConversionIntentEnabledInner(true, (Intent intent) -> {
            mCustomTabActivityTestRule.prepareUrlIntent(intent, "about:blank");
            IntentUtils.safePutBinderExtra(intent, CustomTabsIntent.EXTRA_SESSION, null);
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        });
    }

    @Test
    @LargeTest
    @Feature({"ConversionMeasurement"})
    @Features.EnableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testConversionIntentEnabled_CCT_preload_sameUrl() {
        AttributionReporter.setInstanceForTesting(mAttributionReporter);
        CustomTabsConnection connection = CustomTabsTestUtils.setUpConnection();
        Context context = ContextUtils.getApplicationContext();
        String url = mCustomTabActivityTestRule.getTestServer().getURL("/echo");
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, url);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        try {
            connection.setCanUseHiddenTabForSession(token, true);
            Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(url), null, null));
            CustomTabsTestUtils.ensureCompletedSpeculationForUrl(connection, url);

            Intent outerIntent = makeValidAttributionIntent(
                    "1234", "https://example.com", "https://example2.com", 5678);
            intent.fillIn(outerIntent, 0);
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
            Mockito.verify(mAttributionReporter, Mockito.times(1))
                    .reportAttributionForCurrentNavigation(any(),
                            eq(ContextUtils.getApplicationContext().getPackageName()), eq("1234"),
                            eq("https://example.com"), eq("https://example2.com"), eq(5678L));
        } finally {
            CustomTabsTestUtils.cleanupSessions(connection);
        }
    }

    @Test
    @LargeTest
    @Feature({"ConversionMeasurement"})
    @Features.EnableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @DisabledTest(message = "Test is flaky. crbug.com/1306267")
    public void testConversionIntentEnabled_CCT_preload_differentUrl() {
        AttributionReporter.setInstanceForTesting(mAttributionReporter);
        CustomTabsConnection connection = CustomTabsTestUtils.setUpConnection();
        Context context = ContextUtils.getApplicationContext();
        String url = mCustomTabActivityTestRule.getTestServer().getURL("/echo");
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, url);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        try {
            connection.setCanUseHiddenTabForSession(token, true);
            String otherUrl = url + "/other.html";
            Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(otherUrl), null, null));
            CustomTabsTestUtils.ensureCompletedSpeculationForUrl(connection, otherUrl);

            doTestConversionIntentEnabledInner(true, (Intent outerIntent) -> {
                intent.fillIn(outerIntent, 0);
                mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
            });
        } finally {
            CustomTabsTestUtils.cleanupSessions(connection);
        }
    }

    @Test
    @LargeTest
    @Feature({"ConversionMeasurement"})
    @Features.DisableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    public void testConversionIntentDisabled() {
        Intent outerIntent = makeValidAttributionIntent("1234", "about:blank", "reportTo", 0);
        mTabbedActivityTestRule.startMainActivityFromIntent(outerIntent, "about:blank");
        Assert.assertNull(mAttributionIntentReceived);
    }

    @Test
    @LargeTest
    @Feature({"ConversionMeasurement"})
    @Features.EnableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    public void testInvalidConversionIntent() {
        Intent outerIntent = makeValidAttributionIntent(null, null, null, 0);
        // Tests that even an invalid Attribution intent processes the original VIEW intent.
        mTabbedActivityTestRule.startMainActivityFromIntent(outerIntent, "about:blank");
        Assert.assertNotNull(mAttributionIntentReceived);
    }

    @Test
    @LargeTest
    @Feature({"ConversionMeasurement"})
    @Features.EnableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    public void testInvalidConversionIntent_noStartupTabPreloader() {
        Intent outerIntent = makeValidAttributionIntent(null, null, null, 0);
        outerIntent.putExtra(StartupTabPreloader.EXTRA_DISABLE_STARTUP_TAB_PRELOADER, true);
        // Tests that even an invalid Attribution intent processes the original VIEW intent.
        mTabbedActivityTestRule.startMainActivityFromIntent(outerIntent, "about:blank");
        Assert.assertNotNull(mAttributionIntentReceived);
    }
}
