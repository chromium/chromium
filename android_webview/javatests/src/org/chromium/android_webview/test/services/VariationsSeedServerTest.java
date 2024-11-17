// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.services.IVariationsSeedServer;
import org.chromium.android_webview.common.services.IVariationsSeedServerCallback;
import org.chromium.android_webview.common.variations.VariationsServiceMetricsHelper;
import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.android_webview.services.VariationsSeedServer;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CallbackHelper;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.concurrent.TimeoutException;

/**
 * Test VariationsSeedServer. These tests are not batched to make sure all unbinded services are
 * properly killed between tests.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
public class VariationsSeedServerTest {
    private File mTempFile;

    private static class StubSeedServerCallback extends IVariationsSeedServerCallback.Stub {
        public CallbackHelper helper = new CallbackHelper();
        public Bundle metrics;

        @Override
        public void reportVariationsServiceMetrics(Bundle metrics) {
            this.metrics = metrics;
            helper.notifyCalled();
        }
    }

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
    public void testGetSeed() throws FileNotFoundException, RemoteException {
        final ParcelFileDescriptor file =
                ParcelFileDescriptor.open(mTempFile, ParcelFileDescriptor.MODE_WRITE_ONLY);
        Intent intent =
                new Intent(ContextUtils.getApplicationContext(), VariationsSeedServer.class);
        try (ServiceConnectionHelper helper =
                new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            IVariationsSeedServer service =
                    IVariationsSeedServer.Stub.asInterface(helper.getBinder());
            // TODO(paulmiller): Test with various oldSeedDate values, after
            // VariationsSeedServer can write actual seeds (with actual date values).
            service.getSeed(file, /* oldSeedDate= */ 0, new StubSeedServerCallback());
        }
    }

    @Test
    @MediumTest
    public void testReportMetrics()
            throws FileNotFoundException, TimeoutException, RemoteException {
        // Update the stamp time to avoid requesting a new seed.
        VariationsUtils.updateStampTime();
        // Write some fake metrics that should be reported during the getSeed IPC.
        Context context = ContextUtils.getApplicationContext();
        VariationsServiceMetricsHelper initialMetrics =
                VariationsServiceMetricsHelper.fromBundle(new Bundle());
        initialMetrics.setJobInterval(6000);
        initialMetrics.setJobQueueTime(1000);
        initialMetrics.setLastEnqueueTime(4);
        initialMetrics.setLastJobStartTime(7);
        Assert.assertTrue(
                "Failed to write initial variations SharedPreferences",
                initialMetrics.writeMetricsToVariationsSharedPreferences(context));

        VariationsSeedServer server = new VariationsSeedServer();
        IBinder binder = server.onBind(null);
        StubSeedServerCallback callback = new StubSeedServerCallback();
        IVariationsSeedServer.Stub.asInterface(binder)
                .getSeed(
                        ParcelFileDescriptor.open(mTempFile, ParcelFileDescriptor.MODE_WRITE_ONLY),
                        /* oldSeedDate= */ 0,
                        callback);

        callback.helper.waitForCallback(
                "Timed out waiting for reportSeedMetrics() to be called", 0);
        VariationsServiceMetricsHelper metrics =
                VariationsServiceMetricsHelper.fromBundle(callback.metrics);
        Assert.assertEquals(6000, metrics.getJobInterval());
        Assert.assertEquals(1000, metrics.getJobQueueTime());
    }
}
