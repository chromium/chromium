// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.contextmenu;

import android.util.Pair;
import android.view.View;

import org.jni_zero.CalledByNative;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.Log;
import org.chromium.components.embedder_support.contextmenu.ContextMenuItemDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulator;
import org.chromium.components.embedder_support.contextmenu.ContextMenuUi;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.List;

/** A helper class that handles generating and dismissing context menus for {@link WebContents}. */
public class AwContextMenuHelper {
    private static final String TAG = "AwContextMenuHelper";
    private final WebContents mWebContents;

    private ContextMenuPopulator mCurrentPopulator;
    private ContextMenuUi mCurrentContextMenu;

    private AwContextMenuHelper(WebContents webContents) {
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
     */
    @CalledByNative
    private void showContextMenu(ContextMenuParams params, View view) {
        if (params.isFile()) return;

        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();

        if (view == null
                || view.getVisibility() != View.VISIBLE
                || view.getParent() == null
                || windowAndroid == null
                || windowAndroid.getActivity().get() == null
                || mCurrentContextMenu != null) {
            Log.w(TAG, "Could not create context menu");
            return;
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
        Runnable onMenuShown = CallbackUtils.emptyRunnable();
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
            Log.w(TAG, "Could not create items for context menu");
            return;
        }

        mCurrentContextMenu = new AwContextMenuCoordinator();

        mCurrentContextMenu.displayMenu(
                windowAndroid, mWebContents, params, items, callback, onMenuShown, onMenuClosed);
    }

    @CalledByNative
    private void dismissContextMenu() {
        if (mCurrentContextMenu != null) {
            mCurrentContextMenu.dismiss();
            mCurrentContextMenu = null;
        }
    }
}
