// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.RelativeLayout;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;

/**
 * This class is responsible for rendering the bottom sheet that shows the post password migration
 * sheet.
 */
class PostPasswordMigrationSheetView implements BottomSheetContent {
    private final BottomSheetController mBottomSheetController;
    private Callback<Integer> mDismissHandler;
    private final RelativeLayout mContentView;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                    assert mDismissHandler != null;
                    mDismissHandler.onResult(reason);
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                }

                @Override
                public void onSheetStateChanged(int newState, int reason) {
                    if (newState != BottomSheetController.SheetState.HIDDEN) return;
                    // This is a fail-safe for cases where onSheetClosed isn't triggered.
                    mDismissHandler.onResult(BottomSheetController.StateChangeReason.NONE);
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                }
            };

    PostPasswordMigrationSheetView(Context context, BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
        mContentView =
                (RelativeLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.post_pwd_migration_sheet, null);
    }

    void setDismissHandler(Callback<Integer> dismissHandler) {
        mDismissHandler = dismissHandler;
    }

    void setVisible(boolean isVisible) {
        if (!isVisible) {
            mBottomSheetController.hideContent(this, true);
            return;
        }
        mBottomSheetController.addObserver(mBottomSheetObserver);
        if (!mBottomSheetController.requestShowContent(this, true)) {
            mBottomSheetController.removeObserver(mBottomSheetObserver);
            assert mDismissHandler != null;
            mDismissHandler.onResult(BottomSheetController.StateChangeReason.NONE);
        }
    }

    @Nullable
    @Override
    public View getContentView() {
        return mContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        // TODO(b/324025495): Introduce and use proper string.
        return android.R.string.ok;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // TODO(b/324025495): Introduce and use proper string.
        return android.R.string.ok;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(b/324025495): Introduce and use proper string.
        return android.R.string.ok;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(b/324025495): Introduce and use proper string.
        return android.R.string.ok;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }
}
