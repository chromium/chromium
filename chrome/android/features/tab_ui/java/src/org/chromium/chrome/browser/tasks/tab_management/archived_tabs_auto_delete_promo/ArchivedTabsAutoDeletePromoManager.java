// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.archived_tabs_auto_delete_promo;

import android.content.Context;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.util.UrlUtilities;

/**
 * Helper class to manage the conditions for showing the Auto Delete Archived Tabs Decision Promo
 * and triggering it.
 */
@NullMarked
public class ArchivedTabsAutoDeletePromoManager implements Destroyable {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final TabArchiveSettings mTabArchiveSettings;
    private final ObservableSupplier<Integer> mArchivedTabCountSupplier;
    private final TabModel mTabModel;
    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                    onDidSelectTab(tab);
                }
            };
    private @Nullable ArchivedTabsAutoDeletePromoCoordinator
            mArchivedTabsAutoDeletePromoCoordinator;

    /**
     * Constructor.
     *
     * @param context The Android Context.
     * @param bottomSheetController The BottomSheetController for showing the promo.
     * @param tabArchiveSettings The TabArchiveSettings instance.
     * @param archivedTabCountSupplier Supplier for the count of archived tabs.
     * @param tabModel Regular tab model.
     */
    public ArchivedTabsAutoDeletePromoManager(
            Context context,
            BottomSheetController bottomSheetController,
            TabArchiveSettings tabArchiveSettings,
            ObservableSupplier<Integer> archivedTabCountSupplier,
            TabModel tabModel) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mTabArchiveSettings = tabArchiveSettings;
        mArchivedTabCountSupplier = archivedTabCountSupplier;
        mTabModel = tabModel;
        if (checkConditions()) {
            mTabModel.addObserver(mTabModelObserver);
        }
    }

    /**
     * Attempts to show the Auto Delete Archived Tabs Decision Promo. This method will first verify
     * a set of eligibility conditions (e.g., feature flags, user preferences, archived tab state)
     * by calling an internal check. If all conditions are met, it will attempt to instantiate and
     * display the promo bottom sheet to the user.
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
        mTabModel.removeObserver(mTabModelObserver);
        if (mArchivedTabsAutoDeletePromoCoordinator != null) {
            mArchivedTabsAutoDeletePromoCoordinator.destroy();
            mArchivedTabsAutoDeletePromoCoordinator = null;
        }
    }

    /* Observer logic. */
    private void onDidSelectTab(Tab tab) {
        if (tab != null
                && !tab.isIncognitoBranded()
                && UrlUtilities.isNtpUrl(tab.getUrl())
                && !tab.isClosing()
                && !tab.isHidden()) tryToShowArchivedTabsAutoDeleteDecisionPromo();
    }

    /*
     * Conditions required for the promo to be shown:
     * 1. The auto delete promo is available to the user.
     * 2. The relevant kill switch for this promo is ON.
     * 3. User has not already made a choice via this specific promo.
     * 4. The main archiving feature is enabled.
     * 5. The auto-delete feature (user's choice/default) is currently disabled.
     * 6. There is at least one tab in the archive.
     */
    private boolean checkConditions() {
        return ChromeFeatureList.sAndroidTabDeclutterAutoDelete.isEnabled()
                && ChromeFeatureList.sAndroidTabDeclutterAutoDeleteKillSwitch.isEnabled()
                && !mTabArchiveSettings.getAutoDeleteDecisionMade()
                && mTabArchiveSettings.getArchiveEnabled()
                && !mTabArchiveSettings.isAutoDeleteEnabled()
                && mArchivedTabCountSupplier.get() >= 1;
    }
}
