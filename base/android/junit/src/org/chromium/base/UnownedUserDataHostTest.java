// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.os.Looper;

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
        Thread t =
                new Thread() {
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

    @Test
    public void testUiThreadHandlerReused() {
        UnownedUserDataHost host = new UnownedUserDataHost();
        assertSame(ThreadUtils.getUiThreadHandler(), host.getHandlerForTesting());
    }

    @Test
    public void testBackgroundThreadPreparedLooper() throws InterruptedException {
        AtomicBoolean success = new AtomicBoolean();
        Thread t =
                new Thread() {
                    @Override
                    public void run() {
                        Looper.prepare();
                        UnownedUserDataHost host = new UnownedUserDataHost();
                        assertNotSame(
                                ThreadUtils.getUiThreadHandler(), host.getHandlerForTesting());
                        assertEquals(
                                Looper.myLooper(), host.getHandlerForTesting().getLooper());
                        success.set(true);
                    }
                };
        t.start();
        t.join();

        assertTrue(success.get());
    }
}
