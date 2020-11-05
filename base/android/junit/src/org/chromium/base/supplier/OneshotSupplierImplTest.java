// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowProcess;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for {@link OneshotSupplierImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowProcess.class})
public class OneshotSupplierImplTest {
    private OneshotSupplierImpl<String> mSupplier = new OneshotSupplierImpl<>();

    @Spy
    private Callback<String> mCallback1;
    @Spy
    private Callback<String> mCallback2;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testSet() {
        assertNull(mSupplier.onAvailable(mCallback1));
        assertNull(mSupplier.onAvailable(mCallback2));
        mSupplier.set("answer");

        verify(mCallback1).onResult("answer");
        verify(mCallback2).onResult("answer");
    }

    @Test
    public void testSetBeforeAddObserver() {
        mSupplier.set("answer");

        assertEquals("answer", mSupplier.onAvailable(mCallback1));
        assertEquals("answer", mSupplier.onAvailable(mCallback2));

        verify(mCallback1).onResult("answer");
        verify(mCallback2).onResult("answer");
    }

    @Test
    public void testInterleaved() {
        assertNull(mSupplier.onAvailable(mCallback1));
        mSupplier.set("answer");
        assertEquals("answer", mSupplier.onAvailable(mCallback2));

        verify(mCallback1).onResult("answer");
        verify(mCallback2).onResult("answer");
    }

    @Test
    public void testGet() {
        assertNull(mSupplier.get());
        mSupplier.set("answer");
        assertEquals("answer", mSupplier.get());
    }
}
