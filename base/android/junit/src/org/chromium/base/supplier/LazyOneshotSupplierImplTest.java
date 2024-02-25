// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowProcess;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link LazyOneshotSupplierImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowProcess.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class LazyOneshotSupplierImplTest {
    @Spy
    private LazyOneshotSupplierImpl<String> mSupplier =
            new LazyOneshotSupplierImpl<>() {
                @Override
                public void doSet() {
                    set("answer");
                }
            };

    @Spy private Callback<String> mCallback1;
    @Spy private Callback<String> mCallback2;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testSetBeforeDoSet() {
        assertFalse(mSupplier.hasValue());
        mSupplier.set("answer");

        mSupplier.onAvailable(mCallback1);
        mSupplier.onAvailable(mCallback2);

        assertTrue(mSupplier.hasValue());
        verify(mCallback1).onResult("answer");
        verify(mCallback2).onResult("answer");
        verify(mSupplier, times(0)).doSet();
    }

    @Test
    public void testDoSetCalledOnce() {
        mSupplier.onAvailable(mCallback1);
        verify(mSupplier, times(0)).doSet();

        assertEquals("answer", mSupplier.get());
        assertEquals("answer", mSupplier.get());

        verify(mCallback1).onResult("answer");
        verify(mSupplier).doSet();
    }
}
