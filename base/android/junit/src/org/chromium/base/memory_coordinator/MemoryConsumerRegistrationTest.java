// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.memory_coordinator;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.jni_zero.JniUniquePtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.memory_coordinator.MemoryConsumerRegistration.NativeMemoryConsumerAndroid;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link MemoryConsumerRegistration} and {@link MemoryConsumer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MemoryConsumerRegistrationTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MemoryConsumerRegistration.Natives mNativeMock;
    @Mock private MemoryConsumer mConsumerMock;
    @Mock private JniUniquePtr<NativeMemoryConsumerAndroid> mNativeConsumerMock;

    private MemoryConsumerTraits mTraits;

    @Before
    public void setUp() {
        MemoryConsumerRegistration.resetForTesting();
        MemoryConsumerRegistrationJni.setInstanceForTesting(mNativeMock);

        mTraits =
                new MemoryConsumerTraits.Builder(
                                EstimatedMemoryUsage.SMALL,
                                ReleaseMemoryDataAccess.NONE,
                                InformationRetention.LOSSLESS,
                                ExecutionType.SYNCHRONOUS)
                        .build();
    }

    @After
    public void tearDown() {
        MemoryConsumerRegistration.resetForTesting();
        MemoryConsumerRegistrationJni.setInstanceForTesting(null);
    }

    @Test
    public void testRegistrationLifecycle() {
        MemoryConsumerRegistration.flushPendingRegistrations();

        int packedTraits = mTraits.getPackedTraits();
        when(mNativeMock.register(eq("TestConsumer"), eq(packedTraits), eq(mConsumerMock)))
                .thenReturn(mNativeConsumerMock);

        MemoryConsumerRegistration registration =
                MemoryConsumerRegistration.create("TestConsumer", mTraits, mConsumerMock);
        assertNotNull(registration);
        verify(mNativeMock, times(1)).register("TestConsumer", packedTraits, mConsumerMock);

        registration.close();
        verify(mNativeConsumerMock, times(1)).destroy();

        // Second close should be idempotent and not call destroy again.
        registration.close();
        verify(mNativeConsumerMock, times(1)).destroy();
    }

    @Test
    public void testConsumerCallbackDispatch() {
        MemoryConsumerRegistration.onUpdateMemoryLimit(mConsumerMock, 75);
        verify(mConsumerMock, times(1)).onUpdateMemoryLimit(eq(new MemoryLimit(75)));

        MemoryConsumerRegistration.onReleaseMemory(mConsumerMock);
        verify(mConsumerMock, times(1)).onReleaseMemory();
    }

    @Test
    public void testPreNativeRegistrationFlushesWhenNativeInitialized() {
        MemoryConsumerRegistration registration =
                MemoryConsumerRegistration.create("PreNativeConsumer", mTraits, mConsumerMock);
        assertNotNull(registration);

        // Native registration should not have occurred yet.
        int packedTraits = mTraits.getPackedTraits();
        when(mNativeMock.register(eq("PreNativeConsumer"), eq(packedTraits), eq(mConsumerMock)))
                .thenReturn(mNativeConsumerMock);

        // Simulate native becoming ready.
        MemoryConsumerRegistration.flushPendingRegistrations();

        verify(mNativeMock, times(1)).register("PreNativeConsumer", packedTraits, mConsumerMock);

        registration.close();
        verify(mNativeConsumerMock, times(1)).destroy();
    }

    @Test
    public void testPreNativeRegistrationClosedBeforeFlush() {
        MemoryConsumerRegistration registration =
                MemoryConsumerRegistration.create("PreNativeConsumer", mTraits, mConsumerMock);
        assertNotNull(registration);

        // Close before native initialization.
        registration.close();

        // Simulate native becoming ready.
        MemoryConsumerRegistration.flushPendingRegistrations();

        verify(mNativeMock, times(0)).register(eq("PreNativeConsumer"), anyInt(), any());
    }

    @Test
    public void testPostNativeRegistrationRegistersImmediately() {
        MemoryConsumerRegistration.flushPendingRegistrations();

        int packedTraits = mTraits.getPackedTraits();
        when(mNativeMock.register(eq("PostInitConsumer"), eq(packedTraits), eq(mConsumerMock)))
                .thenReturn(mNativeConsumerMock);

        MemoryConsumerRegistration registration =
                MemoryConsumerRegistration.create("PostInitConsumer", mTraits, mConsumerMock);
        verify(mNativeMock, times(1)).register("PostInitConsumer", packedTraits, mConsumerMock);

        registration.close();
        verify(mNativeConsumerMock, times(1)).destroy();
    }

    @Test
    public void testNativeRegistryDestroyedResetsReadyState() {
        MemoryConsumerRegistration.flushPendingRegistrations();

        // Simulate native registry destruction.
        MemoryConsumerRegistration.onNativeRegistryDestroyed();

        // New registrations should now be queued rather than registering immediately.
        MemoryConsumerRegistration registration =
                MemoryConsumerRegistration.create("QueuedConsumer", mTraits, mConsumerMock);
        verify(mNativeMock, times(0)).register(eq("QueuedConsumer"), anyInt(), any());

        // Re-flushing after native becomes ready again.
        int packedTraits = mTraits.getPackedTraits();
        when(mNativeMock.register(eq("QueuedConsumer"), eq(packedTraits), eq(mConsumerMock)))
                .thenReturn(mNativeConsumerMock);

        MemoryConsumerRegistration.flushPendingRegistrations();
        verify(mNativeMock, times(1)).register("QueuedConsumer", packedTraits, mConsumerMock);

        registration.close();
        verify(mNativeConsumerMock, times(1)).destroy();
    }

    @Test
    public void testPassiveConsumerDoesNotRequireReleaseMemory() {
        class TestPassiveConsumer implements PassiveMemoryConsumer {
            public MemoryLimit lastLimit;

            @Override
            public void onUpdateMemoryLimit(MemoryLimit memoryLimit) {
                lastLimit = memoryLimit;
            }
        }

        TestPassiveConsumer passiveConsumer = new TestPassiveConsumer();
        passiveConsumer.onUpdateMemoryLimit(new MemoryLimit(60));
        assertNotNull(passiveConsumer.lastLimit);
        assertEquals(60, passiveConsumer.lastLimit.getPercent());

        // Calling onReleaseMemory on passive consumer is a safe no-op.
        passiveConsumer.onReleaseMemory();
    }

    @Test
    public void testCloseDuringNativeRegistrationDestroysNativePeer() {
        MemoryConsumerRegistration registration =
                MemoryConsumerRegistration.create("ReentrantConsumer", mTraits, mConsumerMock);

        int packedTraits = mTraits.getPackedTraits();

        when(mNativeMock.register(eq("ReentrantConsumer"), eq(packedTraits), eq(mConsumerMock)))
                .thenAnswer(
                        invocation -> {
                            registration.close();
                            return mNativeConsumerMock;
                        });

        MemoryConsumerRegistration.flushPendingRegistrations();
        verify(mNativeConsumerMock, times(1)).destroy();
    }
}
