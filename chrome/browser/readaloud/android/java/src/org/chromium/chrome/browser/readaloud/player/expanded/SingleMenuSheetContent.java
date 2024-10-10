// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.readaloud.player.Colors;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * BottomSheetContent for Read Aloud player menus where the menu is the only content.
 *
 * <p>This is a temporary class on the way to making a menu sheet that can hold multiple menus
 * (options menu and voice menu on the same sheet).
 */
abstract class SingleMenuSheetContent extends MenuSheetContent {
    protected final Menu mMenu;

    SingleMenuSheetContent(
            Context context,
            BottomSheetContent parent,
            BottomSheetController bottomSheetController,
            int titleStringId) {
        this(context, parent, bottomSheetController, titleStringId, LayoutInflater.from(context));
    }

    @VisibleForTesting
    SingleMenuSheetContent(
            Context context,
            BottomSheetContent parent,
            BottomSheetController bottomSheetController,
            int titleStringId,
            LayoutInflater layoutInflater) {
        super(parent, bottomSheetController);

        mMenu = (Menu) layoutInflater.inflate(R.layout.readaloud_menu, null);
        mMenu.afterInflating(
                () -> {
                    mMenu.setBackPressHandler(this::onBackPressed);
                    mMenu.setTitle(titleStringId);
                });

        // Apply dynamic background color.
        Colors.setBottomSheetContentBackground(mMenu);
        Resources res = context.getResources();
        onOrientationChange(res.getConfiguration().orientation);
    }

    // TODO(b/306426853) Replace this with a BottomSheetObserver.
    @Override
    void notifySheetClosed(BottomSheetContent closingContent) {
        super.notifySheetClosed(closingContent);
        if (closingContent == this) {
            mMenu.getScrollView().scrollTo(0, 0);
        }
    }

    /**
     * To be called when the device orientation changes.
     *
     * @param orientation One of Configuration.ORIENTATION_PORTRAIT or
     *     Configuration.ORIENTATION_LANDSCAPE.
     */
    public void onOrientationChange(int orientation) {
        mMenu.onOrientationChange(orientation);
    }

    @Override
    public View getContentView() {
        return mMenu;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mMenu.getScrollView().getScrollY();
    }

    Menu getMenuForTesting() {
        return mMenu;
    }
}
