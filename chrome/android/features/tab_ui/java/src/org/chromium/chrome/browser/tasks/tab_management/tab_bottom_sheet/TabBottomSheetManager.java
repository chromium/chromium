// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import android.content.Context;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet.TabBottomSheetUtils.TabBottomSheetModes;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.util.UrlUtilities;

/** Helper class to manage the conditions for showing the tab bottom sheet and triggering it. */
@NullMarked
public class TabBottomSheetManager implements Destroyable {
    private final Context mContext;
    private final TabModel mTabModel;
    private final BottomSheetController mBottomSheetController;
    private @Nullable TabBottomSheetCoordinator mTabBottomSheetCoordinator;

    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                    onDidSelectTab(tab);
                }
            };

    /**
     * Constructor.
     *
     * @param context The Android Context.
     * @param tabModel The regular {@link TabModel} for the current session.
     * @param bottomSheetController The BottomSheetController for showing the promo.
     */
    public TabBottomSheetManager(
            Context context, TabModel tabModel, BottomSheetController bottomSheetController) {
        mContext = context;
        mTabModel = tabModel;
        mBottomSheetController = bottomSheetController;

        if (checkConditionsForBottomSheet()) {
            mTabModel.addObserver(mTabModelObserver);
        }
    }

    /**
     * Attempts to show the Tab BottomSheet. This method will first verify a set of eligibility
     * conditions (e.g., feature flags, user preferences) by calling an internal check. If all
     * conditions are met, it will attempt to instantiate and display the promo bottom sheet to the
     * user.
     */
    public void tryToShowBottomSheet() {
        if (checkConditionsForBottomSheet()) {
            if (mTabBottomSheetCoordinator == null) {
                mTabBottomSheetCoordinator =
                        new TabBottomSheetCoordinator(mContext, mBottomSheetController);
            }
            mTabBottomSheetCoordinator.showBottomSheet(
                    /* tabBottomSheetMode= */ TabBottomSheetModes.SIMPLE);
        } else {
            destroy();
        }
    }

    @Override
    public void destroy() {
        if (mTabBottomSheetCoordinator != null) {
            mTabBottomSheetCoordinator.destroy();
            mTabBottomSheetCoordinator = null;
        }
        if (mTabModel != null) {
            mTabModel.removeObserver(mTabModelObserver);
        }
    }

    /* Observer logic. */
    private void onDidSelectTab(Tab tab) {
        if (checkConditionsForTab(tab)) {
            tryToShowBottomSheet();
        }
    }

    // Conditions required for the tab to show the bottomsheet.
    private boolean checkConditionsForTab(Tab tab) {
        return tab != null
                && !tab.isIncognitoBranded()
                && UrlUtilities.isNtpUrl(tab.getUrl())
                && !tab.isClosing()
                && !tab.isHidden();
    }

    // Conditions required for the bottomsheet to be shown.
    private boolean checkConditionsForBottomSheet() {
        return ChromeFeatureList.sTabBottomSheet.isEnabled();
    }

    /* Testing methods */
    public @Nullable TabBottomSheetCoordinator getTabBottomSheetCoordinatorForTesting() {
        return mTabBottomSheetCoordinator;
    }
}
