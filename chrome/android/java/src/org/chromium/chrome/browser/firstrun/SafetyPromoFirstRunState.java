// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.safety_promo.SafetyPromoItem;

/**
 * Carries the selected Safety promo card to the carousel page. It is the card the user picked on
 * the overview page, or the card the FRE resumes the carousel on. A null selection means no card is
 * selected, in which case the carousel page is not shown.
 *
 * <p>Also persists and restores the carousel card the user is on, so that the carousel resumes on
 * it when the FRE is relaunched or recreated.
 */
@NullMarked
public class SafetyPromoFirstRunState {
    private final SettableNullableObservableSupplier<SafetyPromoItem> mSelectedItemSupplier =
            ObservableSuppliers.createNullable();

    public NullableObservableSupplier<SafetyPromoItem> getSelectedItemSupplier() {
        return mSelectedItemSupplier;
    }

    public void setSelectedItem(@Nullable SafetyPromoItem item) {
        mSelectedItemSupplier.set(item);
    }

    public boolean hasSelectedItem() {
        return mSelectedItemSupplier.get() != null;
    }

    /** Persists the carousel card the user is on, to resume the carousel on it. */
    void persistActiveItem(SafetyPromoItem item) {
        // Completing the FRE clears the resume state, which shouldn't be persisted again.
        if (FirstRunStatus.getFirstRunFlowComplete()) return;

        FirstRunStatus.setResumeSafetyPromoItemKey(item.key);
    }

    /**
     * Selects the persisted carousel card if the FRE resumes on the carousel, so that the carousel
     * is shown on it. If the card can't be restored (e.g. the arm changed), changes the persisted
     * resume page to the overview, as the carousel is only shown with a selected card.
     *
     * <p>Must be called once the native and the feature flags are initialized, and before the FRE
     * resumes from the persisted resume page. Otherwise, the carousel is skipped when resuming on
     * it, which completes the FRE as it is the last page.
     */
    void maybeRestoreSelectedItem() {
        if (FirstRunStatus.getResumePage() != MobileFreProgress.SAFETY_PROMO_CAROUSEL_SHOWN) return;

        SafetyPromoItem item =
                FirstRunUtils.getSafetyFrePromoCarouselItem(
                        FirstRunStatus.getResumeSafetyPromoItemKey());
        if (item != null) {
            setSelectedItem(item);
        } else {
            FirstRunStatus.setResumePage(MobileFreProgress.SAFETY_PROMO_SHOWN);
        }
    }
}
