// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import androidx.test.filters.SmallTest;

import org.junit.After;
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

    @Test
    @SmallTest
    public void testConsumer() {
        Linker linker = Mockito.spy(new ModernLinker());

        Mockito.doNothing().when(linker).loadLinkerJniLibraryLocked();

        long someAddress = 1 << 12;
        linker.initAsRelroConsumer(someAddress);

        Mockito.verify(linker).keepMemoryReservationUntilLoad();
        Mockito.verify(mNativeMock)
                .reserveMemoryForLibrary(ArgumentMatchers.any(Linker.LibInfo.class));
    }
}
