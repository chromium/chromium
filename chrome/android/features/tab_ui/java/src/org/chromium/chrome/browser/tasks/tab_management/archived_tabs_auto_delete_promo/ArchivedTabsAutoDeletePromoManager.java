// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.archived_tabs_auto_delete_promo;

import android.content.Context;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Helper class to manage the conditions for showing the Auto Delete Archived Tabs Decision Promo
 * and triggering it.
 */
@NullMarked
public class ArchivedTabsAutoDeletePromoManager implements Destroyable {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final TabArchiveSettings mTabArchiveSettings;
    private final NonNullObservableSupplier<Integer> mArchivedTabCountSupplier;
    private @Nullable ArchivedTabsAutoDeletePromoCoordinator
            mArchivedTabsAutoDeletePromoCoordinator;

    /**
     * Constructor.
     *
     * @param context The Android Context.
     * @param bottomSheetController The BottomSheetController for showing the promo.
     * @param tabArchiveSettings The TabArchiveSettings instance.
     * @param archivedTabCountSupplier Supplier for the count of archived tabs.
     */
    public ArchivedTabsAutoDeletePromoManager(
            Context context,
            BottomSheetController bottomSheetController,
            TabArchiveSettings tabArchiveSettings,
            NonNullObservableSupplier<Integer> archivedTabCountSupplier) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mTabArchiveSettings = tabArchiveSettings;
        mArchivedTabCountSupplier = archivedTabCountSupplier;
    }

    /**
     * Attempts to show the Auto Delete Archived Tabs Decision Promo. This method will first verify
     * eligibility conditions (user preferences and archived tab count). If all conditions are met,
     * it will instantiate (if needed) and display the promo bottom sheet to the user; otherwise, it
     * will clean up any existing promo coordinator.
     */
    public void tryToShowArchivedTabsAutoDeleteDecisionPromo() {
        if (checkConditions()) {
            // All conditions met to consider showing the promo.
            if (mArchivedTabsAutoDeletePromoCoordinator == null) {
                mArchivedTabsAutoDeletePromoCoordinator =
                        new ArchivedTabsAutoDeletePromoCoordinator(
                                mContext, mBottomSheetController, mTabArchiveSettings);
            }
            mArchivedTabsAutoDeletePromoCoordinator.showPromo();
        } else {
            destroy();
        }
    }

    @Override
    public void destroy() {
        if (mArchivedTabsAutoDeletePromoCoordinator != null) {
            mArchivedTabsAutoDeletePromoCoordinator.destroy();
            mArchivedTabsAutoDeletePromoCoordinator = null;
        }
    }

    /*
     * Conditions required for the promo to be shown:
     * 1. User has not already made a choice via this specific promo.
     * 2. The main archiving feature is enabled.
     * 3. The auto-delete feature (user's choice/default) is currently disabled.
     * 4. There is at least one tab in the archive.
     */
    private boolean checkConditions() {
        return !mTabArchiveSettings.getAutoDeleteDecisionMade()
                && mTabArchiveSettings.getArchiveEnabled()
                && !mTabArchiveSettings.isAutoDeleteEnabled()
                && mArchivedTabCountSupplier.get() >= 1;
    }
}
