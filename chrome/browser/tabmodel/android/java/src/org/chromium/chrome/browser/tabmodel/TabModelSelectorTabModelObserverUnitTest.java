// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.CallbackHelper;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for the TabModelSelectorTabModelObserver. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabModelSelectorTabModelObserverUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabModelSelector mSelector;

    @Mock private TabModel mTabModel;
    @Captor private ArgumentCaptor<TabModelSelectorTabModelObserver> mArgCaptor;

    private List<TabModel> mTabModels = new ArrayList<>();
    private SettableMonotonicObservableSupplier<TabModel> mTabModelSupplier;

    @Before
    public void setUp() {
        mTabModels = new ArrayList<>();
        mTabModelSupplier = ObservableSuppliers.createMonotonic();
        doReturn(mTabModelSupplier).when(mSelector).getCurrentTabModelSupplier();
        doReturn(mTabModels).when(mSelector).getModels();
    }

    @Test
    public void testAlreadyInitializedSelector() throws TimeoutException {
        // ARRANGE
        mTabModels.add(mTabModel);
        mTabModelSupplier.set(mTabModel);

        // ACT
        final CallbackHelper registrationCompleteCallback = new CallbackHelper();
        TabModelSelectorTabModelObserver observer =
                new TabModelSelectorTabModelObserver(mSelector) {
                    @Override
                    protected void onRegistrationComplete() {
                        registrationCompleteCallback.notifyCalled();
                    }
                };

        // ASSERT
        RobolectricUtil.runAllBackgroundAndUi();
        registrationCompleteCallback.waitForCallback(0);
        verify(mTabModel).addObserver(mArgCaptor.capture());
        assertEquals(1, mTabModels.size());
        assertSame(observer, mArgCaptor.getValue());
    }

    @Test
    public void testUninitializedSelector() throws TimeoutException {
        // ARRANGE

        // ACT
        final CallbackHelper registrationCompleteCallback = new CallbackHelper();
        TabModelSelectorTabModelObserver observer =
                new TabModelSelectorTabModelObserver(mSelector) {
                    @Override
                    protected void onRegistrationComplete() {
                        registrationCompleteCallback.notifyCalled();
                    }
                };
        mTabModels.add(mTabModel); // Ensure a (any) tab model is added after initialization.
        mTabModelSupplier.set(mTabModel);

        // ASSERT
        RobolectricUtil.runAllBackgroundAndUi();
        registrationCompleteCallback.waitForCallback(0);
        verify(mTabModel).addObserver(mArgCaptor.capture());
        assertEquals(1, mTabModels.size());
        assertSame(observer, mArgCaptor.getValue());
    }

    @Test
    public void testDestroySelector() {
        // ARRANGE
        TabModelSelectorTabModelObserver observer = new TabModelSelectorTabModelObserver(mSelector);

        // ACT
        mTabModels.add(mTabModel);
        observer.destroy();

        // ASSERT
        verify(mTabModel).removeObserver(observer);
    }
}
