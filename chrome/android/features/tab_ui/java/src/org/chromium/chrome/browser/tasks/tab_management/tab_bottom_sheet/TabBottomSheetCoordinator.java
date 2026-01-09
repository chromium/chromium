// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the tab bottom sheet. */
@NullMarked
public class TabBottomSheetCoordinator {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final PropertyModel mModel;

    private @Nullable TabBottomSheetContent mSheetContent;
    private @Nullable BottomSheetObserver mSheetObserver;
    private @Nullable PropertyModelChangeProcessor mViewBinder;

    private boolean mIsSheetCurrentlyManagedByController;

    /**
     * Constructor.
     *
     * @param context The Android {@link Context}.
     * @param bottomSheetController The system {@link BottomSheetController}.
     */
    public TabBottomSheetCoordinator(Context context, BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;

        mModel = TabBottomSheetProperties.createDefaultModel();
        setModelProperties();
    }

    /** Shows the bottom sheet. */
    public void showBottomSheet() {
        if (mIsSheetCurrentlyManagedByController) {
            return;
        }
        // Build the bottom sheet.
        View contentView = LayoutInflater.from(mContext).inflate(R.layout.tab_bottom_sheet, null);
        mViewBinder =
                PropertyModelChangeProcessor.create(
                        mModel, contentView, TabBottomSheetViewBinder::bind);
        mSheetContent = new TabBottomSheetContent(contentView);
        mSheetObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                        destroy();
                    }
                };

        if (mBottomSheetController.requestShowContent(mSheetContent, true)) {
            mBottomSheetController.addObserver(mSheetObserver);
            mIsSheetCurrentlyManagedByController = true;
        } else {
            // This happens when either
            // 1) if the sheet content is null
            // 2) The bottom sheet is null
            // 3) If its being shown, or is in queue but not currently shown
            // 4) If a sheet of higher priority came up
            cleanupSheetResources();
        }
    }

    private void setModelProperties() {
        mModel.set(TabBottomSheetProperties.FUSEBOX_ENABLED, true);
    }

    // Cleanup methods.
    public void destroy() {
        if (mIsSheetCurrentlyManagedByController && mSheetContent != null) {
            mBottomSheetController.hideContent(mSheetContent, false, StateChangeReason.NONE);
        }
        cleanupSheetResources();
    }

    private void cleanupSheetResources() {
        if (mSheetObserver != null && mBottomSheetController != null) {
            mBottomSheetController.removeObserver(mSheetObserver);
            mSheetObserver = null;
        }
        if (mSheetContent != null) {
            mSheetContent.destroy();
            mSheetContent = null;
        }
        if (mViewBinder != null) {
            mViewBinder.destroy();
            mViewBinder = null;
        }
        mIsSheetCurrentlyManagedByController = false;
    }

    // Testing methods.
    PropertyModel getModelForTesting() {
        return mModel;
    }

    boolean isSheetCurrentlyManagedForTesting() {
        return mIsSheetCurrentlyManagedByController;
    }
}
