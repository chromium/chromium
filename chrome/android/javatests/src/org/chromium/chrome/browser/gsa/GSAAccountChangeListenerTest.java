// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gsa;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

/** Tests for GSAAccountChangeListener. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class GSAAccountChangeListenerTest {
    private static final String ACCOUNT_NAME = "me@gmail.com";
    private static final String ACCOUNT_NAME2 = "you@gmail.com";
    private static final String PERMISSION = "permission.you.dont.have";

    @Before
    public void setUp() {
        RecordHistogram.setDisabledForTests(true);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testReceivesBroadcastIntents() {
        final Context context = InstrumentationRegistry.getTargetContext();
        BroadcastReceiver receiver = new GSAAccountChangeListener.AccountChangeBroadcastReceiver();
        context.registerReceiver(receiver,
                new IntentFilter(GSAAccountChangeListener.ACCOUNT_UPDATE_BROADCAST_INTENT));

        // Send a broadcast without the permission, should be received.
        Intent intent = new Intent();
        intent.setPackage(context.getPackageName());
        intent.setAction(GSAAccountChangeListener.ACCOUNT_UPDATE_BROADCAST_INTENT);
        intent.putExtra(GSAAccountChangeListener.BROADCAST_INTENT_ACCOUNT_NAME_EXTRA, ACCOUNT_NAME);
        context.sendBroadcast(intent);

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                String currentAccount =
                        GSAState.getInstance(context.getApplicationContext()).getGsaAccount();
                return ACCOUNT_NAME.equals(currentAccount);
            }
        });

        // A broadcast with a permission that Chrome doesn't hold should not be received.
        context.registerReceiver(receiver,
                new IntentFilter(GSAAccountChangeListener.ACCOUNT_UPDATE_BROADCAST_INTENT),
                PERMISSION, null);
        intent.putExtra(
                GSAAccountChangeListener.BROADCAST_INTENT_ACCOUNT_NAME_EXTRA, ACCOUNT_NAME2);
        context.sendBroadcast(intent, "permission.you.dont.have");

        // This is ugly, but so is checking that some asynchronous call was never received.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                String currentAccount =
                        GSAState.getInstance(context.getApplicationContext()).getGsaAccount();
                return ACCOUNT_NAME2.equals(currentAccount);
            }
        }, 1000, 100);
    }
}
