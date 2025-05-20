// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import android.content.res.Resources;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.base.Log;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Base class for menu bottom sheets. */
@NullMarked
abstract class MenuSheetContent implements BottomSheetContent {
    private static final String TAG = "ReadAloudMenu";
    protected final BottomSheetController mBottomSheetController;
    protected final BottomSheetContent mParent;
    private boolean mOpeningSubmenu;

    /**
     * Constructor.
     *
     * @param parent BottomSheetContent to be restored when this sheet hides.
     * @param bottomSheetController BottomSheetController managing this sheet.
     */
    MenuSheetContent(BottomSheetContent parent, BottomSheetController bottomSheetController) {
        mParent = parent;
        mBottomSheetController = bottomSheetController;
        mOpeningSubmenu = false;
    }

    // TODO(b/306426853) Replace this with a BottomSheetObserver.
    void notifySheetClosed(BottomSheetContent closingContent) {
        if (closingContent == this) {
            // If this sheet is closing for any reason besides showing a child menu, bring back the
            // parent.
            if (!mOpeningSubmenu) {
                mBottomSheetController.requestShowContent(mParent, /* animate= */ false);
            }
        }
    }

    protected void openSheet(BottomSheetContent sheet) {
        if (sheet != mParent) {
            mOpeningSubmenu = true;
        }
        mBottomSheetController.hideContent(this, /* animate= */ false);
        mOpeningSubmenu = false;
    }

    protected BottomSheetController getBottomSheetController() {
        return mBottomSheetController;
    }

    @Override
    @Nullable
    public View getToolbarView() {
        return null;
    }

    @Override
    public void destroy() {}

    @Override
    @ContentPriority
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public boolean hasCustomLifecycle() {
        // Dismiss if the user navigates the page, switches tabs, or changes layout.
        return false;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        return false;
    }

    @Override
    public float getHalfHeightRatio() {
        // Only full height mode enabled.
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public boolean handleBackPress() {
        onBackPressed();
        return true;
    }

    @Override
    public ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
        ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
        supplier.set(true);
        return supplier;
    }

    @Override
    public void onBackPressed() {
        openSheet(mParent);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        Log.e(
                TAG,
                "Tried to get half height accessibility string, but half height isn't supported.");
        assert false;
        return Resources.ID_NULL;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        // "Read Aloud player opened at full height."
        return R.string.readaloud_player_opened_at_full_height;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        // "Read Aloud player minimized."
        return R.string.readaloud_player_minimized;
    }

    @Override
    public boolean canSuppressInAnyState() {
        // Always immediately hide if a higher-priority sheet content wants to show.
        return true;
    }
}
