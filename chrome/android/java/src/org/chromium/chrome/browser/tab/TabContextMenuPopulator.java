// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;
import android.util.Pair;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.chrome.browser.contextmenu.ContextMenuImageFormat;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.List;

/**
 * A simple wrapper around a {@link ContextMenuPopulator} to handle observer notification.
 */
public class TabContextMenuPopulator implements ContextMenuPopulator {
    @Nullable
    private final ContextMenuPopulator mPopulator;
    private final TabImpl mTab;

    /**
     * Constructs an instance of a {@link ContextMenuPopulator} and delegate calls to
     * {@code populator}.
     * @param populator The {@link ContextMenuPopulator} to delegate calls to.
     * @param tab The {@link Tab} that is using this context menu.
     */
    public TabContextMenuPopulator(ContextMenuPopulator populator, Tab tab) {
        mPopulator = populator;
        mTab = (TabImpl) tab;
    }

    @Override
    public void onDestroy() {
        // |mPopulator| can be null for activities that do not use context menu. Following
        // methods are not called, but |onDestroy| is.
        if (mPopulator != null) mPopulator.onDestroy();
    }

    @Override
    public List<Pair<Integer, ModelList>> buildContextMenu(
            Context context, ContextMenuParams params, boolean isShoppyImage) {
        List<Pair<Integer, ModelList>> itemGroups =
                mPopulator.buildContextMenu(context, params, isShoppyImage);
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) {
            observers.next().onContextMenuShown(mTab);
        }
        return itemGroups;
    }

    @Override
    public boolean onItemSelected(
            ContextMenuParams params, RenderFrameHost renderFrameHost, int itemId) {
        return mPopulator.onItemSelected(params, renderFrameHost, itemId);
    }

    @Override
    public void getThumbnail(RenderFrameHost renderFrameHost, final Callback<Bitmap> callback) {
        mPopulator.getThumbnail(renderFrameHost, callback);
    }

    @Override
    public void retrieveImage(RenderFrameHost renderFrameHost,
            @ContextMenuImageFormat int imageFormat, Callback<Uri> callback) {
        mPopulator.retrieveImage(renderFrameHost, imageFormat, callback);
    }

    @Override
    public void onMenuClosed() {
        mPopulator.onMenuClosed();
    }
}
