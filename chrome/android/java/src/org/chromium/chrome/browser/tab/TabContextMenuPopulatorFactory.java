// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;

import org.chromium.components.embedder_support.contextmenu.ContextMenuNativeDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulator;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;

/**
 * A simple wrapper around a {@link ContextMenuPopulatorFactory} for creating {@link
 * TabContextMenuPopulator} which is able to handle observer notifications.
 */
class TabContextMenuPopulatorFactory implements ContextMenuPopulatorFactory {
    private final ContextMenuPopulatorFactory mPopulatorFactory;
    private final Tab mTab;

    /**
     * Constructs an instance of {@link TabContextMenuPopulatorFactory}.
     * @param populatorFactory The {@link ContextMenuPopulatorFactory} to delegate the calls to.
     * @param tab The {@link Tab} that is using the populated context menus.
     */
    public TabContextMenuPopulatorFactory(ContextMenuPopulatorFactory populatorFactory, Tab tab) {
        mPopulatorFactory = populatorFactory;
        mTab = tab;
    }

    @Override
    public void onDestroy() {
        // |mPopulatorFactory| can be null for activities that do not use context menu.
        if (mPopulatorFactory != null) mPopulatorFactory.onDestroy();
    }

    @Override
    public boolean isEnabled() {
        return mPopulatorFactory != null;
    }

    @Override
    public ContextMenuPopulator createContextMenuPopulator(
            Context context, ContextMenuParams params, ContextMenuNativeDelegate nativeDelegate) {
        return new TabContextMenuPopulator(
                mPopulatorFactory.createContextMenuPopulator(context, params, nativeDelegate),
                mTab);
    }
}
