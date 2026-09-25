// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.memory_coordinator;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link MemoryConsumerRegistration} and {@link MemoryConsumer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MemoryConsumerRegistrationTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MemoryConsumerRegistration.Natives mNativeMock;
    @Mock private MemoryConsumer mConsumerMock;

    private MemoryConsumerTraits mTraits;

    @Before
    public void setUp() {
        MemoryConsumerRegistration.resetForTesting();
        MemoryConsumerRegistrationJni.setInstanceForTesting(mNativeMock);

        mTraits =
                new MemoryConsumerTraits.Builder(
                                EstimatedMemoryUsage.SMALL,
                                ReleaseMemoryCost.FREES_PAGES_WITHOUT_TRAVERSAL,
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

        long fakeNativePtr = 0x12345678L;
        when(mNativeMock.register(eq("TestConsumer"), eq(mTraits), eq(mConsumerMock)))
                .thenReturn(fakeNativePtr);

        MemoryConsumerRegistration registration =
                MemoryConsumerRegistration.create("TestConsumer", mTraits, mConsumerMock);
        assertNotNull(registration);
        verify(mNativeMock, times(1)).register("TestConsumer", mTraits, mConsumerMock);
        verify(mNativeMock, times(1)).notifyInitialLimitIfNonDefault(fakeNativePtr);

        registration.close();
        verify(mNativeMock, times(1)).destroy(fakeNativePtr);

        // Second close should be idempotent and not call destroy again.
        registration.close();
        verify(mNativeMock, times(1)).destroy(fakeNativePtr);
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
        long fakeNativePtr = 0xabcdefL;
        when(mNativeMock.register(eq("PreNativeConsumer"), eq(mTraits), eq(mConsumerMock)))
                .thenReturn(fakeNativePtr);

        // Simulate native becoming ready.
        MemoryConsumerRegistration.flushPendingRegistrations();

        verify(mNativeMock, times(1)).register("PreNativeConsumer", mTraits, mConsumerMock);
        verify(mNativeMock, times(1)).notifyInitialLimitIfNonDefault(fakeNativePtr);

        registration.close();
        verify(mNativeMock, times(1)).destroy(fakeNativePtr);
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

        verify(mNativeMock, times(0)).register(eq("PreNativeConsumer"), any(), any());
    }

    @Test
    public void testPostNativeRegistrationRegistersImmediately() {
        MemoryConsumerRegistration.flushPendingRegistrations();

        long fakeNativePtr = 0x5678L;
        when(mNativeMock.register(eq("PostInitConsumer"), eq(mTraits), eq(mConsumerMock)))
                .thenReturn(fakeNativePtr);

        MemoryConsumerRegistration registration =
                MemoryConsumerRegistration.create("PostInitConsumer", mTraits, mConsumerMock);
        verify(mNativeMock, times(1)).register("PostInitConsumer", mTraits, mConsumerMock);
        verify(mNativeMock, times(1)).notifyInitialLimitIfNonDefault(fakeNativePtr);

        registration.close();
        verify(mNativeMock, times(1)).destroy(fakeNativePtr);
    }

    @Test
    public void testNativeRegistryDestroyedResetsReadyState() {
        MemoryConsumerRegistration.flushPendingRegistrations();

        // Simulate native registry destruction.
        MemoryConsumerRegistration.onNativeRegistryDestroyed();

        // New registrations should now be queued rather than registering immediately.
        MemoryConsumerRegistration registration =
                MemoryConsumerRegistration.create("QueuedConsumer", mTraits, mConsumerMock);
        verify(mNativeMock, times(0)).register(eq("QueuedConsumer"), any(), any());

        // Re-flushing after native becomes ready again.
        long fakeNativePtr = 0x9999L;
        when(mNativeMock.register(eq("QueuedConsumer"), eq(mTraits), eq(mConsumerMock)))
                .thenReturn(fakeNativePtr);

        MemoryConsumerRegistration.flushPendingRegistrations();
        verify(mNativeMock, times(1)).register("QueuedConsumer", mTraits, mConsumerMock);
        verify(mNativeMock, times(1)).notifyInitialLimitIfNonDefault(fakeNativePtr);

        registration.close();
        verify(mNativeMock, times(1)).destroy(fakeNativePtr);
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
}
