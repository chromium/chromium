// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link OverridableTabCount}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OverridableTabCountUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabModelSelector mTabModelSelector;
    private final SettableMonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableNonNullObservableSupplier<Integer> mTabModelSelectorTabCountSupplier =
            ObservableSuppliers.createNonNull(0);
    private OverridableTabCount mOverridableTabCount;

    @Before
    public void setUp() {
        when(mTabModelSelector.getCurrentModelTabCountSupplier())
                .thenReturn(mTabModelSelectorTabCountSupplier);
        mOverridableTabCount = new OverridableTabCount(mTabModelSelectorSupplier);
        mTabModelSelectorSupplier.set(mTabModelSelector);
    }

    @Test
    public void testTabCountSupplier() {
        mTabModelSelectorTabCountSupplier.set(1);
        assertEquals(1, mOverridableTabCount.get());
        assertFalse(mOverridableTabCount.hasTokensForTesting());

        mTabModelSelectorTabCountSupplier.set(10);
        assertEquals(10, mOverridableTabCount.get());
        assertFalse(mOverridableTabCount.hasTokensForTesting());

        mTabModelSelectorTabCountSupplier.set(6);
        assertEquals(6, mOverridableTabCount.get());
        assertFalse(mOverridableTabCount.hasTokensForTesting());
    }

    @Test
    public void testOverridableTabCount() {
        int token = mOverridableTabCount.setCount(4);
        assertEquals(4, mOverridableTabCount.get());
        assertTrue(mOverridableTabCount.hasTokensForTesting());

        mTabModelSelectorTabCountSupplier.set(10);
        assertEquals(4, mOverridableTabCount.get());
        assertTrue(mOverridableTabCount.hasTokensForTesting());

        mOverridableTabCount.releaseCount(token);
        assertEquals(10, mOverridableTabCount.get());
        assertFalse(mOverridableTabCount.hasTokensForTesting());
    }

    @Test
    public void testMultipleTokens() {
        int token1 = mOverridableTabCount.setCount(4);
        assertEquals(4, mOverridableTabCount.get());
        assertTrue(mOverridableTabCount.hasTokensForTesting());

        int token2 = mOverridableTabCount.setCount(5);
        assertEquals(5, mOverridableTabCount.get());
        assertTrue(mOverridableTabCount.hasTokensForTesting());

        mTabModelSelectorTabCountSupplier.set(10);
        assertEquals(5, mOverridableTabCount.get());
        assertTrue(mOverridableTabCount.hasTokensForTesting());

        mOverridableTabCount.releaseCount(token1);
        assertEquals(5, mOverridableTabCount.get());
        assertTrue(mOverridableTabCount.hasTokensForTesting());

        mOverridableTabCount.releaseCount(token2);
        assertEquals(10, mOverridableTabCount.get());
        assertFalse(mOverridableTabCount.hasTokensForTesting());
    }
}
