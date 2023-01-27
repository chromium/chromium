// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.util.Pair;

import org.chromium.base.Callback;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter;

import java.util.List;

/**
 * Factory interface for creating {@link ContextMenuPopulator}s.
 */
public interface ContextMenuPopulatorFactory {

    /**
     * Creates a {@ContextMenuPopulator}.
     *
     * @param context        The {@link Context} used to retrieve the strings.
     * @param params         The {@link ContextMenuParams} used to build the context menu.
     * @param nativeDelegate The {@link ContextMenuNativeDelegate} for the context menu.
     * @return The new {@ContextMenuPopulator}.
     */
    ContextMenuPopulator createContextMenuPopulator(
            WindowAndroid windowAndroid, ContextMenuParams params, ContextMenuNativeDelegate nativeDelegate);

    default boolean show(WindowAndroid windowAndroid, WebContents webContents,
                         ContextMenuParams params, List<Pair<Integer, MVCListAdapter.ModelList>> items,
                         ContextMenuNativeDelegate nativeDelegate,
                         Callback<Integer> onItemClicked, final Runnable onMenuShown,
                         final Runnable onMenuClosed) {
        return false;
    }

    void onDestroy();
}
