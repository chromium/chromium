// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ScrollView;
import android.widget.TextView;

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
    private final Context mContext;
    protected final Menu mMenu;
    protected final ScrollView mScrollView;

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
        mContext = context;

        mMenu = (Menu) layoutInflater.inflate(R.layout.readaloud_menu, null);

        mMenu.findViewById(R.id.readaloud_menu_back)
                .setOnClickListener(
                        (view) -> {
                            onBackPressed();
                        });

        mScrollView = (ScrollView) mMenu.findViewById(R.id.items_scroll_view);

        ((TextView) mMenu.findViewById(R.id.readaloud_menu_title))
                .setText(context.getResources().getString(titleStringId));

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
            mScrollView.scrollTo(0, 0);
        }
    }

    /**
     * To be called when the device orientation changes.
     *
     * @param orientation One of Configuration.ORIENTATION_PORTRAIT or
     *     Configuration.ORIENTATION_LANDSCAPE.
     */
    public void onOrientationChange(int orientation) {
        MaxHeightScrollView scrollView = getContentView().findViewById(R.id.items_scroll_view);

        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            scrollView.setMaxHeight(
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.scroll_view_height_portrait));

        } else if (orientation == Configuration.ORIENTATION_LANDSCAPE) {
            scrollView.setMaxHeight(
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.scroll_view_height_landscape));
        }
        mScrollView.invalidate();
    }

    @Override
    public View getContentView() {
        return mMenu;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mScrollView.getScrollY();
    }

    Menu getMenuForTesting() {
        return mMenu;
    }
}
