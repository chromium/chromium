// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.services.AwEntropyState;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;

/** Test webview entropy state is correctly preserved */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS)
@Batch(Batch.UNIT_TESTS)
public class AwEntropyStateTest {

    @Test
    @SmallTest
    public void testGenerateAndPersistEntropy() {
        // Ensure clean state
        AwEntropyState.clearPreferencesForTesting();

        // Should be -1 initially
        Assert.assertEquals(-1, AwEntropyState.getLowEntropySource());

        // Generate (simulating first run)
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwEntropyState.ensureLowEntropySourceInitialized();
                });

        // Should be generated now (not -1)
        int generated = AwEntropyState.getLowEntropySource();
        Assert.assertNotEquals(-1, generated);
    }
}
