// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.os.Handler;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link OneShotCallback}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OneShotCallbackTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    Callback<Integer> mCallbackMock;

    @Test
    public void testNotCalledWithNoValue() {
        Handler handler = new Handler();
        ObservableSupplierImpl<Integer> supplier = new ObservableSupplierImpl<>();

        handler.post(() -> new OneShotCallback<>(supplier, mCallbackMock));

        handler.post(() -> { verify(mCallbackMock, never()).onResult(any()); });
    }

    @Test
    public void testCalledWithPresetValue() {
        Handler handler = new Handler();
        ObservableSupplierImpl<Integer> supplier = new ObservableSupplierImpl<>();
        supplier.set(5);

        handler.post(() -> { new OneShotCallback<>(supplier, mCallbackMock); });

        handler.post(() -> { verify(mCallbackMock, times(1)).onResult(5); });
    }

    @Test
    public void testCalledWithSet() {
        Handler handler = new Handler();
        ObservableSupplierImpl<Integer> supplier = new ObservableSupplierImpl<>();

        handler.post(() -> new OneShotCallback<>(supplier, mCallbackMock));
        handler.post(() -> { verify(mCallbackMock, never()).onResult(any()); });

        supplier.set(5);
        handler.post(() -> { verify(mCallbackMock, times(1)).onResult(5); });
    }

    @Test
    public void testNotCalledWithPresetValueOnlyOnce() {
        Handler handler = new Handler();
        ObservableSupplierImpl<Integer> supplier = new ObservableSupplierImpl<>();
        supplier.set(5);
        supplier.set(10);

        handler.post(() -> { new OneShotCallback<>(supplier, mCallbackMock); });

        handler.post(() -> { verify(mCallbackMock, times(1)).onResult(10); });
    }

    @Test
    public void testCalledWithSetOnlyOnce() {
        Handler handler = new Handler();
        ObservableSupplierImpl<Integer> supplier = new ObservableSupplierImpl<>();

        handler.post(() -> new OneShotCallback<>(supplier, mCallbackMock));
        handler.post(() -> { verify(mCallbackMock, never()).onResult(any()); });

        supplier.set(5);
        handler.post(() -> { verify(mCallbackMock, times(1)).onResult(5); });

        supplier.set(10);
        verifyNoMoreInteractions(mCallbackMock);
    }
}