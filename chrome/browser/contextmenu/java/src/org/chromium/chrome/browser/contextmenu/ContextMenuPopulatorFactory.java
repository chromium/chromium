// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;

import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;

/**
 * Factory interface for creating {@link ContextMenuPopulator}s.
 */
public interface ContextMenuPopulatorFactory {
    /**
     * Creates a {@ContextMenuPopulator}.
     * @param context The {@link Context} used to retrieve the strings.
     * @param params The {@link ContextMenuParams} used to build the context menu.
     * @param nativeDelegate The {@link ContextMenuNativeDelegate} for the context menu.
     * @return The new {@ContextMenuPopulator}.
     */
    ContextMenuPopulator createContextMenuPopulator(
            Context context, ContextMenuParams params, ContextMenuNativeDelegate nativeDelegate);

    void onDestroy();
}
