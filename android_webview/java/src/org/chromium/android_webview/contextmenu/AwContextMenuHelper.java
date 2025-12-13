// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.contextmenu;

import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings.HyperlinkContextMenuItems;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** A helper class that handles generating and dismissing context menus for {@link WebContents}. */
@NullMarked
public class AwContextMenuHelper {
    private static final String TAG = "AwContextMenuHelper";
    private final WebContents mWebContents;

    public @Nullable AwContextMenuCoordinator mCurrentContextMenu;

    @VisibleForTesting
    public AwContextMenuHelper(WebContents webContents) {
        mWebContents = webContents;
    }

    @CalledByNative
    private static AwContextMenuHelper create(WebContents webContents) {
        return new AwContextMenuHelper(webContents);
    }

    @CalledByNative
    private void destroy() {
        dismissContextMenu();
    }

    /**
     * Starts showing a context menu for {@code view} based on {@code params}.
     *
     * @param params The {@link ContextMenuParams} that indicate what menu items to show.
     * @param view container view for the menu.
     * @return whether the menu was displayed.
     */
    @CalledByNative
    @VisibleForTesting
    public boolean showContextMenu(ContextMenuParams params, View view) {
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        mCurrentContextMenu = null;

        if (!params.isAnchor()
                || view == null
                || view.getVisibility() != View.VISIBLE
                || view.getParent() == null
                || windowAndroid == null
                || windowAndroid.getActivity().get() == null
                || windowAndroid.getContext().get() == null) {
            Log.w(TAG, "Could not create context menu");
            return false;
        }

        boolean isDragDropEnabled =
                ContextMenuUtils.isDragDropEnabled(windowAndroid.getContext().get());
        // Determine whether to display the context menu as an anchored popup window or a dialog.
        // The usePopupWindow variable is set to true if:
        // 1. Drag and drop is enabled:
        //    - ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU flag is enabled AND
        //    - The device is considered a tablet OR
        //    - The command line flag FORCE_CONTEXT_MENU_POPUP is enabled.
        // 2. OR the context menu was triggered by a mouse right-click or by a shared highlight
        //    interaction.
        // Otherwise, usePopupWindow is false, and the menu is displayed as a dialog.
        boolean usePopupWindow =
                isDragDropEnabled || ContextMenuUtils.isMouseOrHighlightPopup(params);

        AwContents awContents = AwContents.fromWebContents(mWebContents);
        @HyperlinkContextMenuItems
        int hyperlinkMenuItems = getHyperlinkContextMenuItems(awContents);

        mCurrentContextMenu =
                new AwContextMenuCoordinator(
                        windowAndroid,
                        mWebContents,
                        params,
                        isDragDropEnabled,
                        usePopupWindow,
                        hyperlinkMenuItems);

        mCurrentContextMenu.displayMenu();
        return true;
    }

    public @Nullable AwContextMenuCoordinator getCoordinatorForTesting() {
        return mCurrentContextMenu;
    }

    @CalledByNative
    private void dismissContextMenu() {
        if (mCurrentContextMenu != null) {
            mCurrentContextMenu.dismiss();
            mCurrentContextMenu = null;
        }
    }

    protected @HyperlinkContextMenuItems int getHyperlinkContextMenuItems(AwContents awContents) {
        return awContents.getSettings().getHyperlinkContextMenuItems();
    }
}
