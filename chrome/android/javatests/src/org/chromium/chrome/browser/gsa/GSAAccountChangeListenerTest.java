// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gsa;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Tests for GSAAccountChangeListener. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class GSAAccountChangeListenerTest {
    private static final String ACCOUNT_NAME = "me@gmail.com";
    private static final String ACCOUNT_NAME2 = "you@gmail.com";
    private static final String PERMISSION = "permission.you.dont.have";

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

        CriteriaHelper.pollUiThread(() -> {
            String currentAccount = GSAState.getInstance().getGsaAccount();
            Criteria.checkThat(currentAccount, Matchers.is(ACCOUNT_NAME));
        });

        // A broadcast with a permission that Chrome doesn't hold should not be received.
        context.registerReceiver(receiver,
                new IntentFilter(GSAAccountChangeListener.ACCOUNT_UPDATE_BROADCAST_INTENT),
                PERMISSION, null);
        intent.putExtra(
                GSAAccountChangeListener.BROADCAST_INTENT_ACCOUNT_NAME_EXTRA, ACCOUNT_NAME2);
        context.sendBroadcast(intent, "permission.you.dont.have");

        // This is ugly, but so is checking that some asynchronous call was never received.
        CriteriaHelper.pollUiThread(() -> {
            String currentAccount = GSAState.getInstance().getGsaAccount();
            Criteria.checkThat(currentAccount, Matchers.is(ACCOUNT_NAME2));
        }, 1000, 100);
    }
}
