// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowProcess;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;

/** Unit tests for {@link OneshotSupplierImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowProcess.class})
public class OneshotSupplierImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private final OneshotSupplierImpl<String> mSupplier = new OneshotSupplierImpl<>();

    @Spy private Callback<String> mCallback1;
    @Spy private Callback<String> mCallback2;

    @Test
    public void testSet() {
        assertNull(mSupplier.onAvailable(mCallback1));
        assertNull(mSupplier.onAvailable(mCallback2));
        mSupplier.set("answer");

        RobolectricUtil.runAllBackgroundAndUi();
        verify(mCallback1).onResult("answer");
        verify(mCallback2).onResult("answer");
    }

    @Test
    public void testSetBeforeAddObserver() {
        mSupplier.set("answer");

        assertEquals("answer", mSupplier.onAvailable(mCallback1));
        assertEquals("answer", mSupplier.onAvailable(mCallback2));

        RobolectricUtil.runAllBackgroundAndUi();
        verify(mCallback1).onResult("answer");
        verify(mCallback2).onResult("answer");
    }

    @Test
    public void testInterleaved() {
        assertNull(mSupplier.onAvailable(mCallback1));
        mSupplier.set("answer");
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals("answer", mSupplier.onAvailable(mCallback2));

        RobolectricUtil.runAllBackgroundAndUi();
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
