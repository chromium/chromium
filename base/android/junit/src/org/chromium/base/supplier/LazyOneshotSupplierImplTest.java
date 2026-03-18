// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
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

/** Unit tests for {@link LazyOneshotSupplierImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowProcess.class})
public class LazyOneshotSupplierImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

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

    @Test
    public void testSetBeforeDoSet() {
        assertFalse(mSupplier.hasValue());
        mSupplier.set("answer");

        mSupplier.onAvailable(mCallback1);
        mSupplier.onAvailable(mCallback2);

        RobolectricUtil.runAllBackgroundAndUi();
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

        RobolectricUtil.runAllBackgroundAndUi();
        verify(mCallback1).onResult("answer");
        verify(mSupplier).doSet();
    }
}
