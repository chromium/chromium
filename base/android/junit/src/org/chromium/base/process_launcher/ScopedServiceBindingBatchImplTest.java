// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import android.content.Context;
import android.content.ServiceConnection;
import android.os.Build.VERSION_CODES;
import android.os.Handler;
import android.os.Looper;

import androidx.annotation.RequiresApi;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.BaseFeatures;
import org.chromium.base.BindingRequestQueue;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.MinAndroidSdkLevel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests for {@link ScopedBatchUpdate}. */
@RunWith(BaseRobolectricTestRunner.class)
// Requires VERSION_CODES.UPSIDE_DOWN_CAKE for Context.BindServiceFlags.of() in
// BindService.doRebindService().
@Config(
        shadows = {ShadowLooper.class},
        sdk = VERSION_CODES.UPSIDE_DOWN_CAKE)
@Features.EnableFeatures({
    BaseFeatures.REBINDING_CHILD_SERVICE_CONNECTION_CONTROLLER,
    BaseFeatures.REBIND_SERVICE_BATCH_API
})
public class ScopedServiceBindingBatchImplTest {
    private static class FakeBindingRequestQueue implements BindingRequestQueue {
        private final List<ServiceConnection> mRebinds = new ArrayList<>();
        private final List<ServiceConnection> mUnbinds = new ArrayList<>();
        // Ordered log of the calls made on this queue, used to assert that an urgent request is
        // enqueued before the queue is flushed.
        private final List<String> mEvents = new ArrayList<>();
        private int mFlushCount;

        @Override
        public void rebind(ServiceConnection connection, Context.BindServiceFlags flags) {
            mRebinds.add(connection);
            mEvents.add("rebind");
        }

        @Override
        public void unbind(ServiceConnection connection) {
            mUnbinds.add(connection);
            mEvents.add("unbind");
        }

        @Override
        public void flush() {
            mFlushCount++;
            mEvents.add("flush");
        }

        List<ServiceConnection> getRebinds() {
            return mRebinds;
        }

        List<ServiceConnection> getUnbinds() {
            return mUnbinds;
        }

        int getFlushCount() {
            return mFlushCount;
        }

        List<String> getEvents() {
            return mEvents;
        }

        void clearEvents() {
            mEvents.clear();
        }
    }

    private FakeBindingRequestQueue mFakeBindingRequestQueue;
    private Handler mLauncherHandler;
    private ShadowLooper mLauncherLooper;

    @Before
    public void setUp() {
        mFakeBindingRequestQueue = new FakeBindingRequestQueue();
        ScopedServiceBindingBatchImpl.setBindingRequestQueueForTesting(mFakeBindingRequestQueue);

        // ScopedServiceBindingBatchImpl uses a handler to post tasks to the launcher thread.
        // We can use a shadow looper to control the execution of these tasks.
        mLauncherLooper = ShadowLooper.getShadowMainLooper();
        mLauncherHandler = new Handler(Looper.getMainLooper());
    }

    @After
    public void tearDown() {
        ScopedServiceBindingBatchImpl.clearContextForTesting();
        ScopedServiceBindingBatchImpl.setBindingRequestQueueForTesting(null);
    }

    @Test
    @MinAndroidSdkLevel(VERSION_CODES.UPSIDE_DOWN_CAKE)
    @RequiresApi(VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testRebindService_queuesRequest() {
        assertTrue(ScopedServiceBindingBatchImpl.tryActivate(mLauncherHandler));
        ServiceConnection conn = mock(ServiceConnection.class);
        int lastFlushCount;
        try (ScopedServiceBindingBatchImpl batch = ScopedServiceBindingBatchImpl.scoped()) {
            assertNotNull(batch);
            mLauncherLooper.runToEndOfTasks(); // Process beginOnLauncherThread

            BindService.doRebindService(
                    ContextUtils.getApplicationContext(), conn, 0, /* urgent= */ false);

            // The request should be queued and not flushed.
            assertEquals(1, mFakeBindingRequestQueue.getRebinds().size());
            assertEquals(conn, mFakeBindingRequestQueue.getRebinds().get(0));
            mLauncherLooper.runToEndOfTasks(); // Process beginOnLauncherThread
            lastFlushCount = mFakeBindingRequestQueue.getFlushCount();
        }

        mLauncherLooper.runToEndOfTasks(); // Process endOnLauncherThread
        assertEquals(lastFlushCount + 1, mFakeBindingRequestQueue.getFlushCount());
    }

    @Test
    @MinAndroidSdkLevel(VERSION_CODES.UPSIDE_DOWN_CAKE)
    @RequiresApi(VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testRebindService_urgentRequestIsFlushedWithinScope() {
        assertTrue(ScopedServiceBindingBatchImpl.tryActivate(mLauncherHandler));
        ServiceConnection conn = mock(ServiceConnection.class);
        try (ScopedServiceBindingBatchImpl batch = ScopedServiceBindingBatchImpl.scoped()) {
            assertNotNull(batch);
            mLauncherLooper.runToEndOfTasks(); // Process beginOnLauncherThread
            int flushCountBefore = mFakeBindingRequestQueue.getFlushCount();

            BindService.doRebindService(
                    ContextUtils.getApplicationContext(), conn, 0, /* urgent= */ true);

            // An urgent request raises the priority of a process which is about to paint, so it
            // must not wait for the scope to be closed.
            assertEquals(1, mFakeBindingRequestQueue.getRebinds().size());
            assertEquals(conn, mFakeBindingRequestQueue.getRebinds().get(0));
            assertEquals(flushCountBefore + 1, mFakeBindingRequestQueue.getFlushCount());
        }
    }

    @Test
    @MinAndroidSdkLevel(VERSION_CODES.UPSIDE_DOWN_CAKE)
    @RequiresApi(VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testRebindService_urgentRequestIsEnqueuedBeforeFlushing() {
        assertTrue(ScopedServiceBindingBatchImpl.tryActivate(mLauncherHandler));
        ServiceConnection conn = mock(ServiceConnection.class);
        try (ScopedServiceBindingBatchImpl batch = ScopedServiceBindingBatchImpl.scoped()) {
            assertNotNull(batch);
            mLauncherLooper.runToEndOfTasks(); // Process beginOnLauncherThread
            mFakeBindingRequestQueue.clearEvents();

            BindService.doRebindService(
                    ContextUtils.getApplicationContext(), conn, 0, /* urgent= */ false);
            BindService.doRebindService(
                    ContextUtils.getApplicationContext(), conn, 0, /* urgent= */ true);

            // The queue is keyed by ServiceConnection. Sending the urgent request directly instead
            // of enqueueing it would leave the earlier request for the same connection in the
            // queue, and that stale request would overwrite the urgent flags on the next flush.
            assertEquals(
                    Arrays.asList("rebind", "rebind", "flush"),
                    mFakeBindingRequestQueue.getEvents());
        }
    }

    @Test
    @MinAndroidSdkLevel(VERSION_CODES.UPSIDE_DOWN_CAKE)
    @RequiresApi(VERSION_CODES.UPSIDE_DOWN_CAKE)
    @Features.EnableFeatures({BaseFeatures.REBIND_SERVICE_BATCH_API + ":flush-on-upgrade/false"})
    public void testRebindService_urgentRequestStaysQueuedWhenParamDisabled() {
        assertTrue(ScopedServiceBindingBatchImpl.tryActivate(mLauncherHandler));
        ServiceConnection conn = mock(ServiceConnection.class);
        try (ScopedServiceBindingBatchImpl batch = ScopedServiceBindingBatchImpl.scoped()) {
            assertNotNull(batch);
            mLauncherLooper.runToEndOfTasks(); // Process beginOnLauncherThread
            int flushCountBefore = mFakeBindingRequestQueue.getFlushCount();

            BindService.doRebindService(
                    ContextUtils.getApplicationContext(), conn, 0, /* urgent= */ true);

            assertEquals(1, mFakeBindingRequestQueue.getRebinds().size());
            assertEquals(flushCountBefore, mFakeBindingRequestQueue.getFlushCount());
        }
    }

    @Test
    public void testUnbindService_queuesRequest() {
        assertTrue(ScopedServiceBindingBatchImpl.tryActivate(mLauncherHandler));
        ServiceConnection conn = mock(ServiceConnection.class);
        int lastFlushCount;
        try (ScopedServiceBindingBatchImpl batch = ScopedServiceBindingBatchImpl.scoped()) {
            assertNotNull(batch);
            mLauncherLooper.runToEndOfTasks(); // Process beginOnLauncherThread

            BindService.doUnbindService(ContextUtils.getApplicationContext(), conn);

            // The request should be queued and not flushed.
            assertEquals(1, mFakeBindingRequestQueue.getUnbinds().size());
            assertEquals(conn, mFakeBindingRequestQueue.getUnbinds().get(0));
            mLauncherLooper.runToEndOfTasks(); // Process beginOnLauncherThread
            lastFlushCount = mFakeBindingRequestQueue.getFlushCount();
        }

        mLauncherLooper.runToEndOfTasks(); // Process endOnLauncherThread
        assertEquals(lastFlushCount + 1, mFakeBindingRequestQueue.getFlushCount());
    }

    @Test
    @MinAndroidSdkLevel(VERSION_CODES.UPSIDE_DOWN_CAKE)
    @RequiresApi(VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testNestedScopedServiceBindingBatchImpls() {
        assertTrue(ScopedServiceBindingBatchImpl.tryActivate(mLauncherHandler));
        ServiceConnection conn1 = mock(ServiceConnection.class);
        ServiceConnection conn2 = mock(ServiceConnection.class);
        ServiceConnection conn3 = mock(ServiceConnection.class);

        int lastFlushCount;
        try (ScopedServiceBindingBatchImpl batch1 = ScopedServiceBindingBatchImpl.scoped()) {
            assertNotNull(batch1);
            mLauncherLooper.runToEndOfTasks(); // Process beginOnLauncherThread for batch1

            BindService.doRebindService(
                    ContextUtils.getApplicationContext(), conn1, 0, /* urgent= */ false);

            try (ScopedServiceBindingBatchImpl batch2 = ScopedServiceBindingBatchImpl.scoped()) {
                assertNotNull(batch2);
                mLauncherLooper.runToEndOfTasks(); // Process beginOnLauncherThread for batch2

                BindService.doUnbindService(ContextUtils.getApplicationContext(), conn2);
            } // batch2 closed, endOnLauncherThread for batch2 is posted.

            mLauncherLooper.runToEndOfTasks(); // Process endOnLauncherThread for batch2
            assertEquals(1, mFakeBindingRequestQueue.getRebinds().size());
            assertEquals(1, mFakeBindingRequestQueue.getUnbinds().size());

            BindService.doRebindService(
                    ContextUtils.getApplicationContext(), conn3, 0, /* urgent= */ false);
            mLauncherLooper.runToEndOfTasks();
            lastFlushCount = mFakeBindingRequestQueue.getFlushCount();
        } // batch1 closed, endOnLauncherThread for batch1 is posted.

        mLauncherLooper.runToEndOfTasks(); // Process endOnLauncherThread for batch1
        assertEquals(2, mFakeBindingRequestQueue.getRebinds().size());
        assertEquals(conn1, mFakeBindingRequestQueue.getRebinds().get(0));
        assertEquals(conn3, mFakeBindingRequestQueue.getRebinds().get(1));
        assertEquals(1, mFakeBindingRequestQueue.getUnbinds().size());
        assertEquals(conn2, mFakeBindingRequestQueue.getUnbinds().get(0));
        assertEquals(lastFlushCount + 1, mFakeBindingRequestQueue.getFlushCount());
    }

    @Test
    @Features.DisableFeatures(BaseFeatures.REBIND_SERVICE_BATCH_API)
    public void testFeatureDisabled() {
        ScopedServiceBindingBatchImpl.setBindingRequestQueueForTesting(null);
        assertFalse(ScopedServiceBindingBatchImpl.tryActivate(mLauncherHandler));

        try (ScopedServiceBindingBatchImpl batch = ScopedServiceBindingBatchImpl.scoped()) {
            assertNull(batch);
        }

        // The service binding calls should not be batched.
        ServiceConnection conn = mock(ServiceConnection.class);
        BindService.doUnbindService(ContextUtils.getApplicationContext(), conn);
        assertEquals(0, mFakeBindingRequestQueue.getRebinds().size());
        assertEquals(0, mFakeBindingRequestQueue.getUnbinds().size());
        assertEquals(0, mFakeBindingRequestQueue.getFlushCount());
    }
}
