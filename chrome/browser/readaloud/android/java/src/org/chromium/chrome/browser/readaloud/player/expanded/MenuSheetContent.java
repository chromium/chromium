// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Base class for menu bottom sheets. */
class MenuSheetContent implements BottomSheetContent {
    private static final String TAG = "ReadAloudMenu";
    private final BottomSheetController mBottomSheetController;
    protected final BottomSheetContent mParent;
    private boolean mOpeningSubmenu;
    protected final Menu mMenu;

    /**
     * Constructor.
     *
     * @param context Context.
     * @param titleStringId Resource ID of string to show at the top.
     */
    MenuSheetContent(
            Context context,
            BottomSheetContent parent,
            BottomSheetController bottomSheetController,
            int titleStringId) {
        this(
                context,
                parent,
                bottomSheetController,
                titleStringId,
                (Menu) LayoutInflater.from(context).inflate(R.layout.readaloud_menu, null));
        ((TextView) mMenu.findViewById(R.id.readaloud_menu_title))
                .setText(context.getResources().getString(titleStringId));
    }

    @VisibleForTesting
    MenuSheetContent(
            Context context,
            BottomSheetContent parent,
            BottomSheetController bottomSheetController,
            int titleStringId,
            Menu menu) {
        mParent = parent;
        mBottomSheetController = bottomSheetController;
        mMenu = menu;
        mMenu.findViewById(R.id.readaloud_menu_back)
                .setOnClickListener(
                        (view) -> {
                            onBackPressed();
                        });
        mOpeningSubmenu = false;
    }

    // TODO(b/306426853) Replace this with a BottomSheetObserver.
    void notifySheetClosed(BottomSheetContent closingContent) {
        if (closingContent == this) {
            // If this sheet is closing for any reason besides showing a child menu, bring back the
            // parent.
            if (!mOpeningSubmenu) {
                mBottomSheetController.requestShowContent(mParent, /* animate= */ true);
            }
        }
    }

    protected void openSheet(BottomSheetContent sheet) {
        if (sheet != mParent) {
            mOpeningSubmenu = true;
        }
        mBottomSheetController.hideContent(this, /* animate= */ false);
        mBottomSheetController.requestShowContent(sheet, /* animate= */ true);
        mOpeningSubmenu = false;
    }

    protected BottomSheetController getBottomSheetController() {
        return mBottomSheetController;
    }

    @Override
    public View getContentView() {
        return mMenu;
    }

    @Override
    @Nullable
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
    public int getPeekHeight() {
        // Only full height mode enabled.
        return HeightMode.DISABLED;
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
    public int getSheetContentDescriptionStringId() {
        // "Read Aloud player."
        // Automatically appended: "Swipe down to close."
        return R.string.readaloud_player_name;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        Log.e(
                TAG,
                "Tried to get half height accessibility string, but half height isn't supported.");
        assert false;
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // "Read Aloud player opened at full height."
        return R.string.readaloud_player_opened_at_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // "Read Aloud player minimized."
        return R.string.readaloud_player_minimized;
    }
}
