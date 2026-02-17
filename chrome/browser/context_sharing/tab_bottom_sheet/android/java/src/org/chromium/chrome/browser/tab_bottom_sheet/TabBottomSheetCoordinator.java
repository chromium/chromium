// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.context_sharing.R;
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
    private @Nullable View mContentView;

    private boolean mIsSheetCurrentlyManagedByController;

    /**
     * @param context The Android {@link Context}.
     * @param bottomSheetController The {@link BottomSheetController} used to show the bottom sheet.
     */
    TabBottomSheetCoordinator(Context context, BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;

        mModel = TabBottomSheetProperties.createDefaultModel();
    }

    /** Tries to show the bottom sheet. */
    boolean tryToShowBottomSheet(
            @Nullable View toolbarView, View webUiView, @Nullable View fuseboxView) {
        if (mIsSheetCurrentlyManagedByController) {
            return false;
        }

        // Build the bottom sheet.
        mContentView = LayoutInflater.from(mContext).inflate(R.layout.tab_bottom_sheet, null);
        ViewGroup toolbarContainer = mContentView.findViewById(R.id.toolbar_container);
        ViewGroup webUiContainer = mContentView.findViewById(R.id.web_ui_container);
        ViewGroup fuseboxContainer = mContentView.findViewById(R.id.fusebox_container);

        // Add the views to the bottom sheet.
        if (toolbarView != null) {
            toolbarContainer.addView(toolbarView);
        }
        webUiContainer.addView(webUiView);
        if (fuseboxView != null) {
            fuseboxContainer.addView(fuseboxView);
        }

        mViewBinder =
                PropertyModelChangeProcessor.create(
                        mModel, mContentView, TabBottomSheetViewBinder::bind);
        mSheetContent = new TabBottomSheetContent(mContentView);

        if (mBottomSheetController.requestShowContent(mSheetContent, true)) {
            mSheetObserver = buildBottomSheetObserver();
            mBottomSheetController.addObserver(mSheetObserver);
            mIsSheetCurrentlyManagedByController = true;
            return true;
        } else {
            // This happens when either.
            // 1) If the sheet content is null.
            // 2) The bottom sheet is null.
            // 3) If its being shown, or is in queue but not currently shown.
            // 4) If a sheet of higher priority came up.
            cleanupSheetResources();
            return false;
        }
    }

    void closeBottomSheet() {
        if (!mIsSheetCurrentlyManagedByController) {
            return;
        }
        mBottomSheetController.hideContent(mSheetContent, false, StateChangeReason.NONE);
    }

    // Cleanup methods.
    void destroy() {
        if (mIsSheetCurrentlyManagedByController && mSheetContent != null) {
            mBottomSheetController.hideContent(mSheetContent, false, StateChangeReason.NONE);
        }
        cleanupSheetResources();
    }

    boolean isSheetShowing() {
        return mIsSheetCurrentlyManagedByController;
    }

    private void cleanupSheetResources() {
        // If we inflated content and attached external views, remove them from
        // their containers so those views can be reused later.
        if (mContentView != null) {
            ViewGroup toolbarContainer = mContentView.findViewById(R.id.toolbar_container);
            ViewGroup webUiContainer = mContentView.findViewById(R.id.web_ui_container);
            ViewGroup fuseboxContainer = mContentView.findViewById(R.id.fusebox_container);
            if (toolbarContainer != null) toolbarContainer.removeAllViews();
            if (webUiContainer != null) webUiContainer.removeAllViews();
            if (fuseboxContainer != null) fuseboxContainer.removeAllViews();
            mContentView = null;
        }
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

    // Observer methods.
    private BottomSheetObserver buildBottomSheetObserver() {
        return new EmptyBottomSheetObserver() {
            @Override
            public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
                mModel.set(TabBottomSheetProperties.FUSEBOX_OFFSET, offsetPx);
            }
        };
    }

    // Testing methods.
    PropertyModel getModelForTesting() {
        return mModel;
    }

    boolean isSheetCurrentlyManagedForTesting() {
        return mIsSheetCurrentlyManagedByController;
    }
}
