// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.annotation.SuppressLint;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.base.CancelableRunnable;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.ContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.preloading.AndroidPrerenderManager;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import java.util.Objects;

/**
 * TileView's UI interaction handler, including tile usage via touch and tile modification via
 * context menu.
 */
@NullMarked
class TileInteractionDelegateImpl
        implements TileGroup.TileInteractionDelegate,
                ContextMenuManager.Delegate,
                TileGroup.TileDragHandlerDelegate {
    private final ContextMenuManager mContextMenuManager;
    private final TileGroup.Delegate mTileGroupDelegate;
    private final TileGroup.TileDragDelegate mTileDragDelegate;
    private final TileGroup.CustomTileModificationDelegate mCustomTileModificationDelegate;
    private final int mPrerenderDelay;
    private final Tile mTile;
    private final AndroidPrerenderManager mAndroidPrerenderManager;

    private @Nullable Runnable mOnClickRunnable;
    private @Nullable Runnable mOnRemoveRunnable;
    private @Nullable CancelableRunnable mPrerenderRunnable;
    private @Nullable GURL mPrerenderedUrl;
    private @Nullable GURL mScheduldedPrerenderingUrl;

    public TileInteractionDelegateImpl(
            ContextMenuManager contextMenuManager,
            TileGroup.Delegate tileGroupDelegate,
            TileGroup.TileDragDelegate tileDragDelegate,
            TileGroup.CustomTileModificationDelegate customTileModificationDelegate,
            int prerenderDelay,
            Tile tile,
            View view) {
        mContextMenuManager = contextMenuManager;
        mTileGroupDelegate = tileGroupDelegate;
        mTileDragDelegate = tileDragDelegate;
        mCustomTileModificationDelegate = customTileModificationDelegate;
        mPrerenderDelay = prerenderDelay;
        mTile = tile;
        view.setOnTouchListener(TileInteractionDelegateImpl.this);
        mAndroidPrerenderManager = AndroidPrerenderManager.getAndroidPrerenderManager();

        mTileGroupDelegate.initAndroidPrerenderManager(mAndroidPrerenderManager);
    }

    // TileGroup.TileInteractionDelegate => OnClickListener implementation.
    @Override
    public void onClick(View view) {
        SuggestionsMetrics.recordTileTapped();
        mTileDragDelegate.reset();
        if (mOnClickRunnable != null) mOnClickRunnable.run();
        mTileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.CURRENT_TAB, mTile);
    }

    private void maybePrerender(GURL url) {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.NEW_TAB_PAGE_ANDROID_TRIGGER_FOR_PRERENDER2)) {
            return;
        }

        // Avoid resetting the delayed task if witness several MotionEvent.ACTION_DOWN in a row. If
        // the URL has been scheduled to be prerendered or already prerendered, it should be
        // skipped.
        if (Objects.equals(mScheduldedPrerenderingUrl, url)
                || Objects.equals(mPrerenderedUrl, url)) {
            return;
        }

        assert mScheduldedPrerenderingUrl == null;
        mScheduldedPrerenderingUrl = url;
        mPrerenderRunnable =
                new CancelableRunnable(
                        () -> {
                            if (mAndroidPrerenderManager.startPrerendering(url)) {
                                mPrerenderedUrl = url;
                            }
                            mScheduldedPrerenderingUrl = null;
                        });
        PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, mPrerenderRunnable, mPrerenderDelay);
    }

    // This function cancels scheduled prerendering or calls stopPrerendering to stop stale
    // prerendering.
    private void cancelPrerender() {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.NEW_TAB_PAGE_ANDROID_TRIGGER_FOR_PRERENDER2)) {
            return;
        }

        if (mPrerenderRunnable != null) {
            mPrerenderRunnable.cancel();
            mPrerenderRunnable = null;
        }

        if (mPrerenderedUrl != null) {
            mAndroidPrerenderManager.stopPrerendering();
        }

        mPrerenderedUrl = null;
        mScheduldedPrerenderingUrl = null;
    }

    // TileGroup.TileInteractionDelegate => View.OnCreateContextMenuListener implementation.
    @Override
    public void onCreateContextMenu(
            ContextMenu contextMenu, View view, ContextMenuInfo contextMenuInfo) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TILE_CONTEXT_MENU_REFACTOR)) return;

        mContextMenuManager.createContextMenu(contextMenu, view, this);
    }

    // TileGroup.TileInteractionDelegate => View.OnLongClickListener implementation.
    @Override
    public boolean onLongClick(View view) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TILE_CONTEXT_MENU_REFACTOR)) {
            return false;
        }

        return mContextMenuManager.showListContextMenu(view, this);
    }

    // TileGroup.TileInteractionDelegate => View.OnTouchListener implementation.
    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouch(View view, MotionEvent event) {
        if (event.getAction() == MotionEvent.ACTION_DOWN) {
            maybePrerender(mTile.getUrl());
        } else if (event.getAction() == MotionEvent.ACTION_CANCEL) {
            cancelPrerender();
        }

        // Handle tile drag-and-drop separately.
        if (event.getAction() == MotionEvent.ACTION_DOWN) {
            mTileDragDelegate.onTileTouchDown(view, event, this);
        } else if (mTileDragDelegate.hasSession()) {
            mTileDragDelegate.onSessionTileTouch(view, event);
        }

        return false;
    }

    // TileGroup.TileInteractionDelegate implementation.
    @Override
    public void setOnClickRunnable(Runnable clickRunnable) {
        mOnClickRunnable = clickRunnable;
    }

    @Override
    public void setOnRemoveRunnable(Runnable removeRunnable) {
        mOnRemoveRunnable = removeRunnable;
    }

    // ContextMenuManager.Delegate implementation.
    @Override
    public void openItem(int windowDisposition) {
        mTileGroupDelegate.openMostVisitedItem(windowDisposition, mTile);
    }

    @Override
    public void openItemInGroup(int windowDisposition) {
        mTileGroupDelegate.openMostVisitedItemInGroup(windowDisposition, mTile);
    }

    @Override
    public void removeItem() {
        if (mOnRemoveRunnable != null) mOnRemoveRunnable.run();

        mTileGroupDelegate.removeMostVisitedItem(mTile);
    }

    @Override
    public void pinItem() {
        mCustomTileModificationDelegate.convert(mTile.getData());
    }

    @Override
    public void unpinItem() {
        mCustomTileModificationDelegate.remove(mTile.getData());
    }

    @Override
    public void editItem() {
        mCustomTileModificationDelegate.edit(mTile.getData());
    }

    @Override
    public @Nullable GURL getUrl() {
        return mTile.getUrl();
    }

    @Override
    public @Nullable String getContextMenuTitle() {
        return null;
    }

    @Override
    public boolean isItemSupported(@ContextMenuItemId int menuItemId) {
        switch (menuItemId) {
            case ContextMenuItemId.REMOVE:
                return !isCustomizationItemSupported(/* matchIsCustomLink= */ true);
            case ContextMenuItemId.PIN_THIS_SHORTCUT:
                return isCustomizationItemSupported(/* matchIsCustomLink= */ false);
            case ContextMenuItemId.EDIT_SHORTCUT: // Fall through.
            case ContextMenuItemId.UNPIN:
                return isCustomizationItemSupported(/* matchIsCustomLink= */ true);
            default:
                return true;
        }
    }

    @Override
    public boolean hasSpaceForPinnedShortcut() {
        return mCustomTileModificationDelegate.hasSpace();
    }

    @Override
    public void onContextMenuCreated() {}

    // TileGroup.TileDragHandlerDelegate implementation.
    @Override
    public void onDragDominate() {
        mContextMenuManager.hideListContextMenu();
    }

    @Override
    public boolean onDragAccept(SiteSuggestion fromSuggestion, SiteSuggestion toSuggestion) {
        RecordUserAction.record("Suggestions.Drag.ReorderItem");
        return mCustomTileModificationDelegate.reorder(fromSuggestion, toSuggestion);
    }

    boolean isCustomizationItemSupported(boolean matchIsCustomLink) {
        if (!ChromeFeatureList.sMostVisitedTilesCustomization.isEnabled()
                || mTile.getSectionType() != TileSectionType.PERSONALIZED) {
            return false;
        }
        boolean isCustomLink = (mTile.getSource() == TileSource.CUSTOM_LINKS);
        return isCustomLink == matchIsCustomLink;
    }
}
