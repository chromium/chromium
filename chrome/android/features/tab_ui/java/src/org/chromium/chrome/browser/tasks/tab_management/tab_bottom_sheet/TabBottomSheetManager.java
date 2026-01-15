// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import android.content.Context;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Manager class for the tab bottom sheet. */
@NullMarked
public class TabBottomSheetManager implements Destroyable {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private @Nullable TabBottomSheetCoordinator mTabBottomSheetCoordinator;

    /**
     * Constructor.
     *
     * @param context The Android Context.
     * @param bottomSheetController The BottomSheetController for showing the promo.
     */
    public TabBottomSheetManager(Context context, BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
    }

    /**
     * Attempts to show the Tab BottomSheet. This method will first verify a set of eligibility
     * conditions (e.g., feature flags, user preferences) by calling an internal check. If all
     * conditions are met, it will attempt to instantiate and display the promo bottom sheet to the
     * user.
     */
    public void tryToShowBottomSheet(TabBottomSheetToolbar tabBottomSheetToolbar) {
        if (TabBottomSheetUtils.isTabBottomSheetEnabled()) {
            if (mTabBottomSheetCoordinator == null) {
                mTabBottomSheetCoordinator =
                        new TabBottomSheetCoordinator(mContext, mBottomSheetController);
            }
            mTabBottomSheetCoordinator.showBottomSheet(tabBottomSheetToolbar);
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
    }

    /* Testing methods */
    public @Nullable TabBottomSheetCoordinator getTabBottomSheetCoordinatorForTesting() {
        return mTabBottomSheetCoordinator;
    }
}
