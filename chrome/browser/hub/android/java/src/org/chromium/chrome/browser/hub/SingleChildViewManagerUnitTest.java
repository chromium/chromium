// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit test for {@link SingleChildViewManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SingleChildViewManagerUnitTest {
    private final SettableNullableObservableSupplier<View> mViewSupplier =
            ObservableSuppliers.createNullable();
    private final ViewGroup mContainerView = new FrameLayout(ContextUtils.getApplicationContext());
    private final View mView1 = new View(ContextUtils.getApplicationContext());
    private final View mView2 = new View(ContextUtils.getApplicationContext());

    @Test
    public void testDestroyAttached() {
        mContainerView.setVisibility(View.GONE);
        SingleChildViewManager singleChildViewManager =
                new SingleChildViewManager(mContainerView, mViewSupplier);
        assertTrue(mViewSupplier.hasObservers());

        mViewSupplier.set(mView1);
        assertEquals(1, mContainerView.getChildCount());
        assertEquals(mView1, mContainerView.getChildAt(0));
        assertEquals(View.VISIBLE, mContainerView.getVisibility());

        singleChildViewManager.destroy();
        assertEquals(0, mContainerView.getChildCount());
        assertEquals(View.GONE, mContainerView.getVisibility());

        assertFalse(mViewSupplier.hasObservers());
    }

    @Test
    public void testDestroyDetached() {
        SingleChildViewManager singleChildViewManager =
                new SingleChildViewManager(mContainerView, mViewSupplier);
        assertTrue(mViewSupplier.hasObservers());

        // No-op.
        mViewSupplier.set(null);
        assertEquals(View.VISIBLE, mContainerView.getVisibility());

        singleChildViewManager.destroy();
        assertEquals(0, mContainerView.getChildCount());
        assertEquals(View.GONE, mContainerView.getVisibility());

        assertFalse(mViewSupplier.hasObservers());
    }

    @Test
    public void testSwitchViews() {
        mContainerView.setVisibility(View.GONE);
        SingleChildViewManager singleChildViewManager =
                new SingleChildViewManager(mContainerView, mViewSupplier);
        assertTrue(mViewSupplier.hasObservers());

        mViewSupplier.set(mView1);
        assertEquals(1, mContainerView.getChildCount());
        assertEquals(mView1, mContainerView.getChildAt(0));
        assertEquals(View.VISIBLE, mContainerView.getVisibility());

        mContainerView.setVisibility(View.GONE);
        mViewSupplier.set(mView2);
        assertEquals(1, mContainerView.getChildCount());
        assertEquals(mView2, mContainerView.getChildAt(0));
        assertEquals(View.VISIBLE, mContainerView.getVisibility());

        singleChildViewManager.destroy();
        assertEquals(0, mContainerView.getChildCount());
        assertEquals(View.GONE, mContainerView.getVisibility());

        assertFalse(mViewSupplier.hasObservers());
    }
}
