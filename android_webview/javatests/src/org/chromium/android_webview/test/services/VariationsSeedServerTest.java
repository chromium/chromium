// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.ConditionVariable;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.services.IVariationsSeedServer;
import org.chromium.android_webview.services.VariationsSeedServer;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.ContextUtils;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

/**
 * Test VariationsSeedServer.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
public class VariationsSeedServerTest {
    private static final long BINDER_TIMEOUT_MILLIS = 10000;

    private File mTempFile;

    @Before
    public void setUp() throws IOException {
        mTempFile = File.createTempFile("test_variations_seed", null);
    }

    @After
    public void tearDown() {
        Assert.assertTrue("Failed to delete \"" + mTempFile + "\"", mTempFile.delete());
    }

    @Test
    @MediumTest
    public void testGetSeed() throws FileNotFoundException {
        final ConditionVariable getSeedCalled = new ConditionVariable();
        final ParcelFileDescriptor file =
                ParcelFileDescriptor.open(mTempFile, ParcelFileDescriptor.MODE_WRITE_ONLY);

        ServiceConnection connection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                try {
                    // TODO(paulmiller): Test with various oldSeedDate values, after
                    // VariationsSeedServer can write actual seeds (with actual date values).
                    IVariationsSeedServer.Stub.asInterface(service)
                            .getSeed(file, /*oldSeedDate=*/ 0);
                } catch (RemoteException e) {
                    Assert.fail("Faild requesting seed: " + e.getMessage());
                } finally {
                    ContextUtils.getApplicationContext().unbindService(this);
                    getSeedCalled.open();
                }
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {}
        };
        Intent intent = new Intent(
                ContextUtils.getApplicationContext(), VariationsSeedServer.class);

        Assert.assertTrue("Failed to bind to VariationsSeedServer",
                ContextUtils.getApplicationContext()
                        .bindService(intent, connection, Context.BIND_AUTO_CREATE));
        Assert.assertTrue("Timed out waiting for getSeed() to return",
                getSeedCalled.block(BINDER_TIMEOUT_MILLIS));
    }
}
