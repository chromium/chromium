// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the tab bottom sheet. */
@NullMarked
public class TabBottomSheetCoordinator {
    private final BottomSheetController mBottomSheetController;
    private final PropertyModel mModel;
    private final CoBrowseViews mCoBrowseViews;
    private final TabBottomSheetMediator mMediator;

    private @Nullable TabBottomSheetContent mSheetContent;
    private @Nullable BottomSheetObserver mSheetObserver;
    private @Nullable PropertyModelChangeProcessor mViewBinder;
    private @Nullable View mContentView;

    private boolean mIsSheetCurrentlyManagedByController;

    /**
     * @param bottomSheetController The {@link BottomSheetController} used to show the bottom sheet.
     * @param coBrowseViews The views to be displayed within the bottom sheet. These should be
     *     obtained via {@link CoBrowseViewFactory}. Note that these views have a single-use
     *     lifecycle; they are destroyed when the bottom sheet is closed and cannot be reused for
     *     subsequent showings.
     */
    TabBottomSheetCoordinator(
            BottomSheetController bottomSheetController, CoBrowseViews coBrowseViews) {
        mBottomSheetController = bottomSheetController;
        mCoBrowseViews = coBrowseViews;

        mModel = TabBottomSheetProperties.createDefaultModel(coBrowseViews);

        mMediator = new TabBottomSheetMediator(mModel, coBrowseViews);
    }

    /** Tries to show the bottom sheet. */
    boolean tryToShowBottomSheet() {
        if (mIsSheetCurrentlyManagedByController) {
            return false;
        }

        mContentView = mCoBrowseViews.getView();

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
        if (mCoBrowseViews != null) {
            mCoBrowseViews.destroy();
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
                if (!TabBottomSheetUtils.canResizeWebView()) return;

                mMediator.onSheetOffsetChanged(offsetPx);
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
