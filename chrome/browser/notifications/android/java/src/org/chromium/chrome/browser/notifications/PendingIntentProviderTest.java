// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

/** Test pending intent generation. */
// TODO(xingliu): Test R+ when robolectric supports it.
@RunWith(BaseRobolectricTestRunner.class)
public class PendingIntentProviderTest {
    private static final String TEST_ACTION = "TEST_ACTION";
    private Context mContext;

    @Before
    public void setUp() throws Exception {
        ShadowLog.stream = System.out;
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testPendingIntentFlags_AndroidQ() {
        Intent contentIntent = new Intent(TEST_ACTION);

        // By default, FLAG_IMMUTABLE is added from M to R.
        PendingIntentProvider pendingIntentProvider =
                PendingIntentProvider.getBroadcast(
                        mContext, 0, contentIntent, PendingIntent.FLAG_UPDATE_CURRENT);
        String msg = "FLAG_IMMUTABLE should be added by default to the PendingIntent on Q.";
        Assert.assertEquals(
                msg,
                PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT,
                pendingIntentProvider.getFlags());
        pendingIntentProvider = PendingIntentProvider.getActivity(mContext, 0, contentIntent, 0);
        Assert.assertEquals(msg, PendingIntent.FLAG_IMMUTABLE, pendingIntentProvider.getFlags());
        pendingIntentProvider = PendingIntentProvider.getService(mContext, 0, contentIntent, 0);
        Assert.assertEquals(msg, PendingIntent.FLAG_IMMUTABLE, pendingIntentProvider.getFlags());

        msg = "FLAG_IMMUTABLE should not present when mutable is true.";
        pendingIntentProvider =
                PendingIntentProvider.getBroadcast(
                        mContext,
                        0,
                        contentIntent,
                        PendingIntent.FLAG_UPDATE_CURRENT,
                        /* mutable= */ true);
        Assert.assertEquals(
                msg, PendingIntent.FLAG_UPDATE_CURRENT, pendingIntentProvider.getFlags());
        pendingIntentProvider =
                PendingIntentProvider.getActivity(
                        mContext, 0, contentIntent, 0, /* mutable= */ true);
        Assert.assertEquals(msg, 0, pendingIntentProvider.getFlags());
        pendingIntentProvider =
                PendingIntentProvider.getService(
                        mContext, 0, contentIntent, 0, /* mutable= */ true);
        Assert.assertEquals(msg, 0, pendingIntentProvider.getFlags());
    }

    @Test
    public void testRequestCode() {
        Intent contentIntent = new Intent(TEST_ACTION);
        PendingIntentProvider pendingIntentProvider =
                PendingIntentProvider.getBroadcast(
                        mContext,
                        /* requestCode= */ 0,
                        contentIntent,
                        PendingIntent.FLAG_UPDATE_CURRENT);
        Assert.assertEquals(0, pendingIntentProvider.getRequestCode());
        pendingIntentProvider =
                PendingIntentProvider.getBroadcast(
                        mContext,
                        /* requestCode= */ 1,
                        contentIntent,
                        PendingIntent.FLAG_UPDATE_CURRENT);
        Assert.assertEquals(1, pendingIntentProvider.getRequestCode());
    }
}
