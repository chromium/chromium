// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Test class for {@link UnownedUserDataHost}, which also describes typical usage.
 *
 * Most tests for this class is in {@link UnownedUserDataKeyTest}, since the public API is mostly
 * available from {@link UnownedUserDataKey}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UnownedUserDataHostTest {
    @Test
    public void testDestruction() {
        UnownedUserDataHost host = new UnownedUserDataHost();
        host.destroy();
        assertTrue(host.isDestroyed());
    }

    @Test
    public void testUnpreparedLooper() throws InterruptedException {
        AtomicBoolean illegalStateExceptionThrown = new AtomicBoolean();
        Thread t = new Thread() {
            @Override
            public void run() {
                try {
                    // The Looper on this thread is still unprepared, so this should fail.
                    new UnownedUserDataHost();
                } catch (IllegalStateException e) {
                    illegalStateExceptionThrown.set(true);
                }
            }
        };
        t.start();
        t.join();

        assertTrue(illegalStateExceptionThrown.get());
    }
}
