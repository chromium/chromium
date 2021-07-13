// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.library_loader.Linker.PreferAddress;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 *  Tests for {@link Linker}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@SuppressWarnings("GuardedBy") // doNothing().when(...).methodLocked() cannot resolve |mLock|.
public class LinkerTest {
    @Mock
    Linker.Natives mNativeMock;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() {
        Linker.setNativesForTesting(mNativeMock);
    }

    @After
    public void tearDown() {
        Linker.setNativesForTesting(null);
    }

    static Linker.LibInfo anyLibInfo() {
        return ArgumentMatchers.any(Linker.LibInfo.class);
    }

    @Test
    @SmallTest
    public void testConsumer() {
        // Set up.
        Linker linker = Mockito.spy(new ModernLinker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();

        // Exercise.
        long someAddress = 1 << 12;
        linker.ensureInitialized(
                /* asRelroProducer= */ false, PreferAddress.RESERVE_HINT, someAddress);

        // Verify.
        Assert.assertEquals(false, linker.mRelroProducer);
        Mockito.verify(linker).keepMemoryReservationUntilLoad();
        Mockito.verify(mNativeMock).reserveMemoryForLibrary(anyLibInfo());
        Assert.assertNotEquals(null, linker.mLocalLibInfo);
        Assert.assertEquals(someAddress, linker.mLocalLibInfo.mLoadAddress);
    }

    @Test
    @SmallTest
    public void testLegacyConsumer() {
        // Set up.
        Linker linker = Mockito.spy(new LegacyLinker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();

        // Exercise.
        long someAddress = 1 << 12;
        linker.ensureInitialized(
                /* asRelroProducer= */ false, PreferAddress.RESERVE_HINT, someAddress);

        // Verify.
        Assert.assertEquals(false, linker.mRelroProducer);
        Mockito.verify(linker).keepMemoryReservationUntilLoad();
        Mockito.verify(mNativeMock, Mockito.never()).reserveMemoryForLibrary(anyLibInfo());
    }

    @Test
    @SmallTest
    public void testProducer() {
        // Set up.
        Linker linker = Mockito.spy(new ModernLinker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();

        // Exercise.
        linker.ensureInitialized(/* asRelroProducer= */ true, PreferAddress.RESERVE_RANDOM, 0);

        // Verify.
        Assert.assertEquals(true, linker.mRelroProducer);
        Mockito.verify(linker).keepMemoryReservationUntilLoad();
        Mockito.verify(mNativeMock)
                .findMemoryRegionAtRandomAddress(anyLibInfo(), ArgumentMatchers.eq(true));
        Assert.assertNotEquals(null, linker.mLocalLibInfo);
    }

    @Test
    @SmallTest
    public void testConsumerReserveRandom() {
        // Set up.
        Linker linker = Mockito.spy(new ModernLinker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();

        // Exercise.
        linker.ensureInitialized(/* asRelroProducer= */ false, PreferAddress.RESERVE_RANDOM, 0);

        // Verify.
        Mockito.verify(linker).keepMemoryReservationUntilLoad();
        Mockito.verify(mNativeMock)
                .findMemoryRegionAtRandomAddress(anyLibInfo(), ArgumentMatchers.eq(true));
    }

    @Test
    @SmallTest
    public void testReservingZeroFallsBackToRandom() {
        // Set up.
        Linker linker = Mockito.spy(new ModernLinker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();

        // Exercise.
        linker.ensureInitialized(/* asRelroProducer= */ false, PreferAddress.RESERVE_HINT, 0);

        // Verify.
        Mockito.verify(linker).keepMemoryReservationUntilLoad();
        Mockito.verify(mNativeMock)
                .findMemoryRegionAtRandomAddress(anyLibInfo(), ArgumentMatchers.eq(true));
    }

    @Test
    @SmallTest
    public void testAppZygoteProducingRelro() {
        // Set up.
        Linker linker = Mockito.spy(new ModernLinker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();
        // The lookup of the region succeeds.
        Mockito.when(mNativeMock.findRegionReservedByWebViewZygote(anyLibInfo())).thenReturn(true);
        Mockito.when(linker.isNonZeroLoadAddress(anyLibInfo())).thenReturn(true);

        // Exercise.
        linker.ensureInitialized(/* asRelroProducer= */ true, PreferAddress.FIND_RESERVED, 0);

        // Verify.
        Mockito.verify(linker).keepMemoryReservationUntilLoad();
        Mockito.verify(mNativeMock).findRegionReservedByWebViewZygote(anyLibInfo());
        Mockito.verify(mNativeMock, Mockito.never())
                .findMemoryRegionAtRandomAddress(anyLibInfo(), ArgumentMatchers.anyBoolean());
        Mockito.verify(mNativeMock, Mockito.never()).reserveMemoryForLibrary(anyLibInfo());
    }

    @Test
    @SmallTest
    public void testAppZygoteFailsToFindReservedAddressRange() {
        // Set up.
        Linker linker = Mockito.spy(new ModernLinker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();
        // The lookup of the region succeeds.
        Mockito.when(mNativeMock.findRegionReservedByWebViewZygote(anyLibInfo())).thenReturn(false);

        // Exercise.
        linker.ensureInitialized(/* asRelroProducer= */ true, PreferAddress.FIND_RESERVED, 0);

        // Verify.
        Mockito.verify(linker).keepMemoryReservationUntilLoad();
        Mockito.verify(mNativeMock).findRegionReservedByWebViewZygote(anyLibInfo());
        Mockito.verify(mNativeMock)
                .findMemoryRegionAtRandomAddress(anyLibInfo(), ArgumentMatchers.anyBoolean());
    }

    @Test
    @SmallTest
    public void testBrowserExpectingRelroFromZygote() {
        // Set up.
        Linker linker = Mockito.spy(new ModernLinker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();
        // The lookup of the region succeeds.
        Mockito.when(mNativeMock.findRegionReservedByWebViewZygote(anyLibInfo())).thenReturn(true);
        Mockito.when(linker.isNonZeroLoadAddress(anyLibInfo())).thenReturn(true);

        // Exercise.
        linker.ensureInitialized(/* asRelroProducer= */ false, PreferAddress.FIND_RESERVED, 0);

        // Verify.
        Mockito.verify(linker).keepMemoryReservationUntilLoad();
        Mockito.verify(mNativeMock).findRegionReservedByWebViewZygote(anyLibInfo());
        Mockito.verify(mNativeMock, Mockito.never())
                .findMemoryRegionAtRandomAddress(anyLibInfo(), ArgumentMatchers.anyBoolean());
        Mockito.verify(mNativeMock, Mockito.never()).reserveMemoryForLibrary(anyLibInfo());
    }

    @Test
    @SmallTest
    public void testPrivilegedProcessWithHint() {
        // Set up.
        Linker linker = Mockito.spy(new ModernLinker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();
        // The lookup of the region succeeds.
        Mockito.when(mNativeMock.findRegionReservedByWebViewZygote(anyLibInfo())).thenReturn(true);
        Mockito.when(linker.isNonZeroLoadAddress(anyLibInfo())).thenReturn(true);

        // Exercise.
        long someAddress = 1 << 12;
        linker.ensureInitialized(
                /* asRelroProducer= */ false, PreferAddress.FIND_RESERVED, someAddress);

        // Verify.
        Mockito.verify(linker).keepMemoryReservationUntilLoad();
        Mockito.verify(mNativeMock).findRegionReservedByWebViewZygote(anyLibInfo());
        // Unfortunately there does not seem to be an elegant way to set |mLoadAddress| without
        // extracting creation of mLocalLibInfo from ensureInitialized(). Hence no checks are
        // present here involving the exact value of |mLoadAddress|.
    }
}
