// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.view.KeyEvent;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;

/**
 * Tests attribution reporting intents.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@Batch.SplitByFeature
public class AttributionIntentIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private BroadcastReceiver mReceiver;
    private Intent mAttributionIntentReceived;

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
    }

    @After
    public void tearDown() {
        ContextUtils.getApplicationContext().unregisterReceiver(mReceiver);
    }

    private Intent makeValidAttributionIntent(
            String eventId, String destination, String reportTo, String expiry) {
        Intent innerIntent = new Intent(AttributionConstants.ACTION_APP_ATTRIBUTION);
        innerIntent.putExtra(AttributionConstants.EXTRA_ATTRIBUTION_SOURCE_EVENT_ID, eventId);
        innerIntent.putExtra(AttributionConstants.EXTRA_ATTRIBUTION_DESTINATION, destination);
        innerIntent.putExtra(AttributionConstants.EXTRA_ATTRIBUTION_REPORT_TO, reportTo);
        innerIntent.putExtra(AttributionConstants.EXTRA_ATTRIBUTION_EXPIRY, expiry);
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

    @Test
    @LargeTest
    @Feature({"ConversionMeasurement"})
    @Features.EnableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    public void testConversionIntentEnabled() {
        Intent outerIntent = makeValidAttributionIntent("1234", "about:blank", "reportTo", null);
        mActivityTestRule.startMainActivityFromIntent(outerIntent, "about:blank");
        Assert.assertNotNull(mAttributionIntentReceived);
        Assert.assertEquals("1234",
                mAttributionIntentReceived.getStringExtra(
                        AttributionConstants.EXTRA_ATTRIBUTION_SOURCE_EVENT_ID));
        Assert.assertEquals("about:blank",
                mAttributionIntentReceived.getStringExtra(
                        AttributionConstants.EXTRA_ATTRIBUTION_DESTINATION));
        Assert.assertEquals("reportTo",
                mAttributionIntentReceived.getStringExtra(
                        AttributionConstants.EXTRA_ATTRIBUTION_REPORT_TO));
        Assert.assertNull(mAttributionIntentReceived.getStringExtra(
                AttributionConstants.EXTRA_ATTRIBUTION_EXPIRY));
    }

    @Test
    @LargeTest
    @Feature({"ConversionMeasurement"})
    @Features.DisableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    public void testConversionIntentDisabled() {
        Intent outerIntent = makeValidAttributionIntent("1234", "about:blank", "reportTo", null);
        mActivityTestRule.startMainActivityFromIntent(outerIntent, "about:blank");
        Assert.assertNull(mAttributionIntentReceived);
    }

    @Test
    @LargeTest
    @Feature({"ConversionMeasurement"})
    @Features.EnableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    public void testInvalidConversionIntent() {
        Intent outerIntent = makeValidAttributionIntent(null, null, null, null);
        // Tests that even an invalid Attribution intent processes the original VIEW intent.
        mActivityTestRule.startMainActivityFromIntent(outerIntent, "about:blank");
        Assert.assertNotNull(mAttributionIntentReceived);
    }
}
