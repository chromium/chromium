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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.firstrun.FirstRunUtils.SafetyFrePromoArm;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.safety_promo.SafetyPromoItem;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link SafetyPromoFirstRunState}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.SAFETY_FRE_PROMO)
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
        // SafetyPromoFirstRunFragment does when a card is picked on the overview.
        mState.setSelectedItem(null);
        mState.setSelectedItem(SafetyPromoItem.INCOGNITO);
        assertEquals(3, notificationCount.get());
    }

    @Test
    public void testPersistActiveItem() {
        mState.persistActiveItem(SafetyPromoItem.INCOGNITO);

        assertEquals(SafetyPromoItem.INCOGNITO.key, FirstRunStatus.getResumeSafetyPromoItemKey());
    }

    @Test
    public void testPersistActiveItem_firstRunFlowComplete_doesNotPersist() {
        // Completing the FRE clears the resume state. The carousel is paused afterwards, which
        // shouldn't persist the item again.
        FirstRunStatus.setFirstRunFlowComplete(true);

        mState.persistActiveItem(SafetyPromoItem.INCOGNITO);

        assertNull(FirstRunStatus.getResumeSafetyPromoItemKey());
    }

    @Test
    public void testClearResumeState_clearsPersistedItem() {
        mState.persistActiveItem(SafetyPromoItem.INCOGNITO);

        FirstRunStatus.clearResumeState();

        assertNull(FirstRunStatus.getResumeSafetyPromoItemKey());
    }

    @Test
    public void testMaybeRestoreSelectedItem_resumeOnCarousel_selectsPersistedItem() {
        setUpResumeState(
                MobileFreProgress.SAFETY_PROMO_CAROUSEL_SHOWN,
                SafetyPromoItem.ENHANCED_SAFE_BROWSING);

        mState.maybeRestoreSelectedItem();

        assertEquals(
                SafetyPromoItem.ENHANCED_SAFE_BROWSING, mState.getSelectedItemSupplier().get());
        assertEquals(MobileFreProgress.SAFETY_PROMO_CAROUSEL_SHOWN, FirstRunStatus.getResumePage());
    }

    @Test
    public void testMaybeRestoreSelectedItem_itemNotInCarousel_resumesOnOverview() {
        // History Quick Delete is only shown in the carousel of arms 2 and 3.
        setUpResumeState(
                MobileFreProgress.SAFETY_PROMO_CAROUSEL_SHOWN,
                SafetyPromoItem.HISTORY_QUICK_DELETE);

        mState.maybeRestoreSelectedItem();

        assertFalse(mState.hasSelectedItem());
        assertEquals(MobileFreProgress.SAFETY_PROMO_SHOWN, FirstRunStatus.getResumePage());
    }

    @Test
    public void testMaybeRestoreSelectedItem_noPersistedItem_resumesOnOverview() {
        setUpResumeState(MobileFreProgress.SAFETY_PROMO_CAROUSEL_SHOWN, /* item= */ null);

        mState.maybeRestoreSelectedItem();

        assertFalse(mState.hasSelectedItem());
        assertEquals(MobileFreProgress.SAFETY_PROMO_SHOWN, FirstRunStatus.getResumePage());
    }

    @Test
    public void testMaybeRestoreSelectedItem_armWithoutCarousel_resumesOnOverview() {
        setUpResumeState(
                MobileFreProgress.SAFETY_PROMO_CAROUSEL_SHOWN, SafetyPromoItem.PASSWORD_MANAGER);
        // The arm changed to one without the carousel since the item was persisted.
        ChromeFeatureList.sSafetyFrePromoArm.setForTesting(SafetyFrePromoArm.ANIMATED_ILLUSTRATION);

        mState.maybeRestoreSelectedItem();

        assertFalse(mState.hasSelectedItem());
        assertEquals(MobileFreProgress.SAFETY_PROMO_SHOWN, FirstRunStatus.getResumePage());
    }

    @Test
    public void testMaybeRestoreSelectedItem_resumeOnOtherPage_doesNothing() {
        // E.g. the user went back from the carousel to the overview, which keeps the persisted
        // item.
        setUpResumeState(
                MobileFreProgress.SAFETY_PROMO_SHOWN, SafetyPromoItem.ENHANCED_SAFE_BROWSING);

        mState.maybeRestoreSelectedItem();

        assertFalse(mState.hasSelectedItem());
        assertEquals(MobileFreProgress.SAFETY_PROMO_SHOWN, FirstRunStatus.getResumePage());
    }

    /** Persists the resume state of an FRE with the Password Manager arm. */
    private static void setUpResumeState(
            @MobileFreProgress int resumePage, @Nullable SafetyPromoItem item) {
        ChromeFeatureList.sSafetyFrePromoArm.setForTesting(SafetyFrePromoArm.PASSWORD_MANAGER);
        FirstRunStatus.setResumePage(resumePage);
        if (item != null) FirstRunStatus.setResumeSafetyPromoItemKey(item.key);
    }
}
