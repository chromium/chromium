// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for CurrentTabObserver. */
@RunWith(BaseRobolectricTestRunner.class)
public class CurrentTabObserverTest {
    private CurrentTabObserver mCurrentTabObserver;

    private ObservableSupplierImpl<Tab> mTabSupplier;

    @Mock private Tab mTab;

    @Mock private Tab mTab2;

    @Mock private TabObserver mTabObserver;

    @Mock private Callback<Tab> mSwapCallback;

    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);

        mTabSupplier = new ObservableSupplierImpl<>();
        mCurrentTabObserver = new CurrentTabObserver(mTabSupplier, mTabObserver, mSwapCallback);
    }

    @Test
    public void testTriggerWithCurrentTab() {
        mCurrentTabObserver.triggerWithCurrentTab();
        verify(mTab, times(0)).addObserver(mTabObserver);

        // Set the current tab to |mTab|. This adds the observer.
        // The following |triggerWithCurrentTab| invokes the event callback as well
        // but the observer is not added again as the current tab hasn't changed.
        mTabSupplier.set(mTab);
        mCurrentTabObserver.triggerWithCurrentTab();
        verify(mTab, times(1)).addObserver(mTabObserver);
    }

    @Test
    public void testTabObserverAfterTabSwitch() {
        // Make sure the tab observer for this overlay is only observing the "current" tab.
        mTabSupplier.set(mTab);
        verify(mTab).addObserver(mTabObserver);

        mTabSupplier.set(mTab2);
        verify(mTab).removeObserver(mTabObserver);
        verify(mTab2).addObserver(mTabObserver);

        mTabSupplier.set(null);
        verify(mTab2).removeObserver(mTabObserver);
    }

    @Test
    public void testSwapCallback() {
        // When the current tab is swapped, |mSwapCallback| should be notified.
        mTabSupplier.set(mTab);
        verify(mSwapCallback).onResult(mTab);
    }

    @Test
    public void testNullSwapCallback() {
        mCurrentTabObserver.destroy();

        // Null swap callback for CurrentTabObserver should just work without crashing.
        mCurrentTabObserver =
                new CurrentTabObserver(mTabSupplier, mTabObserver, /* swapCallback= */ null);
        mTabSupplier.set(mTab);
        verify(mTab).addObserver(mTabObserver);
        mTabSupplier.set(null);
        verify(mTab).removeObserver(mTabObserver);
    }
}
