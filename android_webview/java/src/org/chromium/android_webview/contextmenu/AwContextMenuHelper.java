// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.contextmenu;

import android.util.Pair;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.contextmenu.ContextMenuItemDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulator;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.List;

/** A helper class that handles generating and dismissing context menus for {@link WebContents}. */
@NullMarked
public class AwContextMenuHelper {
    private static @Nullable Callback<@Nullable AwContextMenuCoordinator>
            sMenuShownCallbackForTesting;
    private static final String TAG = "AwContextMenuHelper";
    private final WebContents mWebContents;

    private @Nullable ContextMenuPopulator mCurrentPopulator;
    private @Nullable AwContextMenuCoordinator mCurrentContextMenu;

    private AwContextMenuHelper(WebContents webContents) {
        mWebContents = webContents;
    }

    @CalledByNative
    @VisibleForTesting
    public static AwContextMenuHelper create(WebContents webContents) {
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

        if (!params.isAnchor()
                || view == null
                || view.getVisibility() != View.VISIBLE
                || view.getParent() == null
                || windowAndroid == null
                || windowAndroid.getActivity().get() == null
                || windowAndroid.getContext().get() == null
                || mCurrentContextMenu != null) {
            Log.w(TAG, "Could not create context menu");
            if (sMenuShownCallbackForTesting != null) {
                sMenuShownCallbackForTesting.onResult(null);
            }
            return false;
        }

        ContextMenuItemDelegate contextMenuItemDelegate =
                new AwContextMenuItemDelegate(
                        windowAndroid.getActivity().get(), mWebContents, params);
        mCurrentPopulator =
                new AwContextMenuPopulator(
                        windowAndroid.getContext().get(), contextMenuItemDelegate, params);
        Callback<Integer> callback =
                (result) -> {
                    if (mCurrentPopulator == null) return;

                    mCurrentPopulator.onItemSelected(result);
                };
        Runnable onMenuShown =
                () -> {
                    if (sMenuShownCallbackForTesting != null) {
                        sMenuShownCallbackForTesting.onResult(mCurrentContextMenu);
                    }
                };
        Runnable onMenuClosed =
                () -> {
                    mCurrentContextMenu = null;
                    if (mCurrentPopulator != null) {
                        mCurrentPopulator.onMenuClosed();
                        mCurrentPopulator = null;
                    }
                };

        // TODO(crbug.com/323344356) make 'Open in browser' disabled by default and only show for
        // HTTP and HTTPS urls
        List<Pair<Integer, ModelList>> items = mCurrentPopulator.buildContextMenu();

        if (items.isEmpty()) {
            if (sMenuShownCallbackForTesting != null) {
                sMenuShownCallbackForTesting.onResult(null);
            }
            Log.w(TAG, "Could not create items for context menu");
            return false;
        }

        mCurrentContextMenu = new AwContextMenuCoordinator();

        mCurrentContextMenu.displayMenu(
                windowAndroid, mWebContents, params, items, callback, onMenuShown, onMenuClosed);
        return true;
    }

    public static void setMenuShownCallbackForTests(
            Callback<@Nullable AwContextMenuCoordinator> callback) {
        sMenuShownCallbackForTesting = callback;
        ResettersForTesting.register(() -> sMenuShownCallbackForTesting = null);
    }

    @CalledByNative
    private void dismissContextMenu() {
        if (mCurrentContextMenu != null) {
            mCurrentContextMenu.dismiss();
            mCurrentContextMenu = null;
        }
    }
}
