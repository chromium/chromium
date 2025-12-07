// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.view.ViewGroup;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit test for {@link SingleChildViewManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SingleChildViewManagerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final ObservableSupplierImpl<View> mViewSupplier = new ObservableSupplierImpl<>();

    @Mock private ViewGroup mContainerView;
    @Mock private View mView1;
    @Mock private View mView2;

    @Test
    public void testDestroyAttached() {
        SingleChildViewManager singleChildViewManager =
                new SingleChildViewManager(mContainerView, mViewSupplier);
        assertTrue(mViewSupplier.hasObservers());

        mViewSupplier.set(mView1);
        verify(mContainerView).addView(mView1);
        verify(mContainerView).setVisibility(View.VISIBLE);
        when(mContainerView.getChildCount()).thenReturn(1);

        singleChildViewManager.destroy();
        verify(mContainerView).removeAllViews();
        verify(mContainerView).setVisibility(View.GONE);

        assertFalse(mViewSupplier.hasObservers());
    }

    @Test
    public void testDestroyDetached() {
        SingleChildViewManager singleChildViewManager =
                new SingleChildViewManager(mContainerView, mViewSupplier);
        assertTrue(mViewSupplier.hasObservers());

        // No-op.
        mViewSupplier.set(null);
        verify(mContainerView, never()).removeAllViews();
        verify(mContainerView, never()).setVisibility(View.GONE);

        singleChildViewManager.destroy();
        verify(mContainerView).removeAllViews();
        verify(mContainerView).setVisibility(View.GONE);

        assertFalse(mViewSupplier.hasObservers());
    }

    @Test
    public void testSwitchViews() {
        SingleChildViewManager singleChildViewManager =
                new SingleChildViewManager(mContainerView, mViewSupplier);
        assertTrue(mViewSupplier.hasObservers());

        mViewSupplier.set(mView1);
        verify(mContainerView).addView(mView1);
        verify(mContainerView).setVisibility(View.VISIBLE);
        when(mContainerView.getChildCount()).thenReturn(1);

        mViewSupplier.set(mView2);
        verify(mContainerView).removeAllViews();
        verify(mContainerView).addView(mView2);
        verify(mContainerView, times(2)).setVisibility(View.VISIBLE);

        singleChildViewManager.destroy();
        verify(mContainerView, times(2)).removeAllViews();
        verify(mContainerView).setVisibility(View.GONE);

        assertFalse(mViewSupplier.hasObservers());
    }
}
