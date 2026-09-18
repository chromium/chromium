// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.safety_promo.SafetyPromoItem;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link SafetyPromoFirstRunState}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyPromoFirstRunStateTest {
    private SafetyPromoFirstRunState mState;

    @Before
    public void setUp() {
        mState = new SafetyPromoFirstRunState();
    }

    @Test
    public void testInitialState() {
        assertFalse(mState.hasSelectedItem());
        assertNull(mState.getSelectedItemSupplier().get());
    }

    @Test
    public void testSetAndGetSelectedItem() {
        mState.setSelectedItem(SafetyPromoItem.PASSWORD_MANAGER);
        assertEquals(SafetyPromoItem.PASSWORD_MANAGER, mState.getSelectedItemSupplier().get());
        assertTrue(mState.hasSelectedItem());

        mState.setSelectedItem(null);
        assertNull(mState.getSelectedItemSupplier().get());
        assertFalse(mState.hasSelectedItem());
    }

    @Test
    public void testSelectedItemSupplier_notifiesObservers() {
        AtomicReference<SafetyPromoItem> observedItem = new AtomicReference<>();
        mState.getSelectedItemSupplier().addSyncObserver(observedItem::set);

        mState.setSelectedItem(SafetyPromoItem.ENHANCED_SAFE_BROWSING);
        assertEquals(SafetyPromoItem.ENHANCED_SAFE_BROWSING, observedItem.get());

        mState.setSelectedItem(null);
        assertNull(observedItem.get());
    }

    @Test
    public void testSelectedItemSupplier_doesNotNotifyForUnchangedValue() {
        AtomicInteger notificationCount = new AtomicInteger();
        mState.getSelectedItemSupplier().addSyncObserver(_ -> notificationCount.incrementAndGet());

        mState.setSelectedItem(SafetyPromoItem.INCOGNITO);
        mState.setSelectedItem(SafetyPromoItem.INCOGNITO);
        assertEquals(1, notificationCount.get());

        // Callers that need to re-apply the same item must clear the selection first. This is what
        // SafetyPromoFirstRunFragment#reset() does when the user navigates back to the overview.
        mState.setSelectedItem(null);
        mState.setSelectedItem(SafetyPromoItem.INCOGNITO);
        assertEquals(3, notificationCount.get());
    }
}
