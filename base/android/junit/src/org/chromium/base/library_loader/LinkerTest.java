// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import android.os.Bundle;

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
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link Linker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@SuppressWarnings("GuardedBy") // doNothing().when(...).methodLocked() cannot resolve |mLock|.
public class LinkerTest {
    @Mock Linker.Natives mNativeMock;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() {
        Linker.setLinkerNativesForTesting(mNativeMock);
    }

    @After
    public void tearDown() {
        Linker.setLinkerNativesForTesting(null);
    }

    static Linker.LibInfo anyLibInfo() {
        return ArgumentMatchers.any(Linker.LibInfo.class);
    }

    @Test
    @SmallTest
    public void testConsumer() {
        // Set up.
        Linker linker = Mockito.spy(new Linker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();

        // Exercise.
        long someAddress = 1 << 12;
        linker.ensureInitialized(
                /* asRelroProducer= */ false, PreferAddress.RESERVE_HINT, someAddress);

        // Verify.
        Assert.assertFalse(linker.mRelroProducer);
        Mockito.verify(mNativeMock).reserveMemoryForLibrary(anyLibInfo());
        Assert.assertNotEquals(null, linker.mLocalLibInfo);
        Assert.assertEquals(someAddress, linker.mLocalLibInfo.mLoadAddress);
    }

    @Test
    @SmallTest
    public void testProducer() {
        // Set up.
        Linker linker = Mockito.spy(new Linker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();

        // Exercise.
        linker.ensureInitialized(/* asRelroProducer= */ true, PreferAddress.RESERVE_RANDOM, 0);

        // Verify.
        Assert.assertTrue(linker.mRelroProducer);
        Mockito.verify(mNativeMock).findMemoryRegionAtRandomAddress(anyLibInfo());
        Assert.assertNotEquals(null, linker.mLocalLibInfo);
    }

    @Test
    @SmallTest
    public void testConsumerReserveRandom() {
        // Set up.
        Linker linker = Mockito.spy(new Linker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();

        // Exercise.
        linker.ensureInitialized(/* asRelroProducer= */ false, PreferAddress.RESERVE_RANDOM, 0);

        // Verify.
        Mockito.verify(mNativeMock).findMemoryRegionAtRandomAddress(anyLibInfo());
    }

    @Test
    @SmallTest
    public void testReservingZeroFallsBackToRandom() {
        // Set up.
        Linker linker = Mockito.spy(new Linker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();

        // Exercise.
        linker.ensureInitialized(/* asRelroProducer= */ false, PreferAddress.RESERVE_HINT, 0);

        // Verify.
        Mockito.verify(mNativeMock).findMemoryRegionAtRandomAddress(anyLibInfo());
    }

    @Test
    @SmallTest
    public void testAppZygoteProducingRelro() {
        // Set up.
        Linker linker = Mockito.spy(new Linker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();
        // The lookup of the region succeeds.
        Mockito.when(mNativeMock.findRegionReservedByWebViewZygote(anyLibInfo())).thenReturn(true);
        Mockito.when(linker.isNonZeroLoadAddress(anyLibInfo())).thenReturn(true);

        // Exercise.
        linker.ensureInitialized(/* asRelroProducer= */ true, PreferAddress.FIND_RESERVED, 0);

        // Verify.
        Mockito.verify(mNativeMock).findRegionReservedByWebViewZygote(anyLibInfo());
        Mockito.verify(mNativeMock, Mockito.never()).findMemoryRegionAtRandomAddress(anyLibInfo());
        Mockito.verify(mNativeMock, Mockito.never()).reserveMemoryForLibrary(anyLibInfo());
    }

    @Test
    @SmallTest
    public void testAppZygoteFailsToFindReservedAddressRange() {
        // Set up.
        Linker linker = Mockito.spy(new Linker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();
        // The lookup of the region fails.
        Mockito.when(mNativeMock.findRegionReservedByWebViewZygote(anyLibInfo())).thenReturn(false);

        // Exercise.
        linker.ensureInitialized(/* asRelroProducer= */ true, PreferAddress.FIND_RESERVED, 0);

        // Verify.
        Mockito.verify(mNativeMock).findRegionReservedByWebViewZygote(anyLibInfo());
        Mockito.verify(mNativeMock).findMemoryRegionAtRandomAddress(anyLibInfo());
    }

    @Test
    @SmallTest
    public void testRelroSharingStatusHistogram() {
        // Set up.
        Linker linker = Mockito.spy(new Linker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();
        Mockito.when(mNativeMock.getRelroSharingResult()).thenReturn(1);
        Linker.LibInfo libInfo = Mockito.spy(new Linker.LibInfo());
        long someAddress = 1 << 12;
        libInfo.mLoadAddress = someAddress;
        // Set a fake RELRO FD so that it is not silently ignored when taking the LibInfo from the
        // (simulated) outside.
        libInfo.mRelroFd = 1023;
        // Create the bundle following the _internal_ format of the Linker. Not great, but shorter
        // than factoring out this logic from the Linker only for testing.
        Bundle relros = libInfo.toBundle();
        Bundle b = new Bundle();
        b.putBundle(Linker.SHARED_RELROS, relros);

        // Exercise.
        linker.ensureInitialized(
                /* asRelroProducer= */ false, PreferAddress.RESERVE_HINT, someAddress);
        linker.pretendLibraryIsLoadedForTesting();
        linker.takeSharedRelrosFromBundle(b);

        // Verify.
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "ChromiumAndroidLinker.RelroSharingStatus2"));
    }

    @Test
    @SmallTest
    public void testBrowserExpectingRelroFromZygote() {
        // Set up.
        Linker linker = Mockito.spy(new Linker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();
        // The lookup of the region succeeds.
        Mockito.when(mNativeMock.findRegionReservedByWebViewZygote(anyLibInfo())).thenReturn(true);
        Mockito.when(linker.isNonZeroLoadAddress(anyLibInfo())).thenReturn(true);

        // Exercise.
        linker.ensureInitialized(/* asRelroProducer= */ false, PreferAddress.FIND_RESERVED, 0);

        // Verify.
        Mockito.verify(mNativeMock).findRegionReservedByWebViewZygote(anyLibInfo());
        Mockito.verify(mNativeMock, Mockito.never()).findMemoryRegionAtRandomAddress(anyLibInfo());
        Mockito.verify(mNativeMock, Mockito.never()).reserveMemoryForLibrary(anyLibInfo());
    }

    @Test
    @SmallTest
    public void testPrivilegedProcessWithHint() {
        // Set up.
        Linker linker = Mockito.spy(new Linker());
        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();
        // The lookup of the region succeeds.
        Mockito.when(mNativeMock.findRegionReservedByWebViewZygote(anyLibInfo())).thenReturn(true);
        Mockito.when(linker.isNonZeroLoadAddress(anyLibInfo())).thenReturn(true);

        // Exercise.
        long someAddress = 1 << 12;
        linker.ensureInitialized(
                /* asRelroProducer= */ false, PreferAddress.FIND_RESERVED, someAddress);

        // Verify.
        Mockito.verify(mNativeMock).findRegionReservedByWebViewZygote(anyLibInfo());
        // Unfortunately there does not seem to be an elegant way to set |mLoadAddress| without
        // extracting creation of mLocalLibInfo from ensureInitialized(). Hence no checks are
        // present here involving the exact value of |mLoadAddress|.
    }
}
