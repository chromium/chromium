// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.annotation.SuppressLint;
import android.os.Build;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.ContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.preloading.AndroidPrerenderManager;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.suggestions.tile.TileDragDelegate.ReorderFlow;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.ui.util.MotionEventUtils;
import org.chromium.url.GURL;

/**
 * TileView's UI interaction handler, including tile usage via touch and tile modification via
 * context menu.
 */
@NullMarked
class TileInteractionDelegateImpl
        implements TileGroup.TileInteractionDelegate,
                ContextMenuManager.Delegate,
                View.OnKeyListener,
                TileDragSession.EventListener {

    private final ContextMenuManager mContextMenuManager;
    private final TileGroup.Delegate mTileGroupDelegate;
    private final TileDragDelegate mTileDragDelegate;
    private final TileGroup.CustomTileModificationDelegate mCustomTileModificationDelegate;
    private final Tile mTile;
    private final View mView;
    private final AndroidPrerenderManager mAndroidPrerenderManager;

    private @Nullable Runnable mOnClickRunnable;
    private @Nullable Runnable mOnRemoveRunnable;
    private boolean mPrerenderTriggered;

    public TileInteractionDelegateImpl(
            ContextMenuManager contextMenuManager,
            TileGroup.Delegate tileGroupDelegate,
            TileDragDelegate tileDragDelegate,
            TileGroup.CustomTileModificationDelegate customTileModificationDelegate,
            Tile tile,
            View view) {
        mContextMenuManager = contextMenuManager;
        mTileGroupDelegate = tileGroupDelegate;
        mTileDragDelegate = tileDragDelegate;
        mCustomTileModificationDelegate = customTileModificationDelegate;
        mTile = tile;
        mView = view;

        mView.setOnClickListener(this);
        mView.setOnKeyListener(this);
        mView.setOnLongClickListener(this);
        mView.setOnTouchListener(this);
        mView.setOnGenericMotionListener(this);

        mAndroidPrerenderManager = AndroidPrerenderManager.getAndroidPrerenderManager();

        mTileGroupDelegate.initAndroidPrerenderManager(mAndroidPrerenderManager);
        mPrerenderTriggered = false;
    }

    // TileGroup.TileInteractionDelegate => OnClickListener implementation.
    @Override
    public void onClick(View view) {
        SuggestionsMetrics.recordTileTapped();
        mTileDragDelegate.reset();
        if (mOnClickRunnable != null) mOnClickRunnable.run();
        mTileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.CURRENT_TAB, mTile);
    }

    // TileGroup.TileInteractionDelegate => View.OnKeyListener implementation.
    @Override
    public boolean onKey(View view, int keyCode, KeyEvent event) {
        if (isCustomizationEnabledForTileSection()
                && TileUtils.isCustomTileSwapKeyCombo(keyCode, event)) {
            if (isCustomLink()) {
                if (event.getAction() == KeyEvent.ACTION_DOWN) {
                    // Complete pending reordering, skipping animation if necessary.
                    mTileDragDelegate.reset();
                } else if (event.getAction() == KeyEvent.ACTION_UP) {
                    // Start swap. Doing it here instead of ACTION_DOWN because if animation is off,
                    // then MVT refresh would cause tile re-render and listener re-add, but then
                    // the original ACTION_DOWN would re-fire! Handling ACTION_UP prevents this.
                    int direction = (keyCode == KeyEvent.KEYCODE_PAGE_UP) ? -1 : 1;
                    mTileDragDelegate.swapTiles(view, direction, this);
                }
            }
            // Suppress Ctrl+Shift+{Page Up, Page Down} propagation, which would lead to undesired
            // page scroll. This includes the following cases:
            // * On ACTION_UP for Custom Tiles: Triggers tile swap (above).
            // * On ACTION_DOWN for Custom Tiles: No-op.
            // * On ACTION_UP / ACTION_DOWN for Top Sites Tiles: No-op, for consistency.
            return true;
        }
        return false;
    }

    // TileGroup.TileInteractionDelegate => View.OnLongClickListener implementation.
    @Override
    public boolean onLongClick(View view) {
        return mContextMenuManager.showListContextMenu(view, this);
    }

    // TileGroup.TileInteractionDelegate => View.OnTouchListener implementation.
    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouch(View view, MotionEvent event) {
        if (event.getAction() == MotionEvent.ACTION_DOWN) {
            mAndroidPrerenderManager.startPrerendering(mTile.getUrl());
            mPrerenderTriggered = true;
        } else if (event.getAction() == MotionEvent.ACTION_CANCEL && mPrerenderTriggered) {
            mAndroidPrerenderManager.stopPrerendering();
            mPrerenderTriggered = false;
        }

        // Handle tile drag-and-drop separately.
        if (event.getAction() == MotionEvent.ACTION_DOWN) {
            mTileDragDelegate.onTileTouchDown(view, event, this);
        } else if (mTileDragDelegate.hasTileDragSession()) {
            mTileDragDelegate.onSessionTileTouch(view, event);
        }

        return false;
    }

    // TileGroup.TileInteractionDelegate => View.OnGenericMotionListener implementation.
    @Override
    public boolean onGenericMotion(View view, MotionEvent event) {
        if (MotionEventUtils.isSecondaryClick(event.getButtonState())
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                && view.hasOnLongClickListeners()) {
            return view.performLongClick();
        }
        return view.onGenericMotionEvent(event);
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
    public void openAllItems() {}

    @Override
    public void removeItem() {
        if (mOnRemoveRunnable != null) mOnRemoveRunnable.run();

        mTileGroupDelegate.removeMostVisitedItem(mTile);
    }

    @Override
    public void removeAllItems() {}

    @Override
    public void pinItem() {
        mCustomTileModificationDelegate.convert(mTile.getData());
    }

    @Override
    public void unpinItem() {
        mCustomTileModificationDelegate.remove(mTile.getData());
    }

    @Override
    public void moveItemUp() {
        mTileDragDelegate.swapTiles(mView, -1, this);
    }

    @Override
    public void moveItemDown() {
        mTileDragDelegate.swapTiles(mView, 1, this);
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
            case ContextMenuItemId.OPEN_IN_NEW_TAB:
                return true;
            case ContextMenuItemId.OPEN_IN_NEW_TAB_IN_GROUP:
                return true;
            case ContextMenuItemId.OPEN_IN_INCOGNITO_TAB:
                return !IncognitoUtils.shouldOpenIncognitoAsWindow();
            case ContextMenuItemId.OPEN_IN_INCOGNITO_WINDOW:
                return IncognitoUtils.shouldOpenIncognitoAsWindow();
            case ContextMenuItemId.OPEN_IN_OTHER_WINDOW:
                return true;
            case ContextMenuItemId.SAVE_FOR_OFFLINE:
                return true;
            case ContextMenuItemId.REMOVE:
                return !isCustomizationItemSupported(/* matchIsCustomLink= */ true);
            case ContextMenuItemId.PIN_THIS_SHORTCUT:
                return isCustomizationItemSupported(/* matchIsCustomLink= */ false);
            case ContextMenuItemId.EDIT_SHORTCUT: // Fall through.
            case ContextMenuItemId.UNPIN:
                return isCustomizationItemSupported(/* matchIsCustomLink= */ true);
            case ContextMenuItemId.MOVE_UP:
                return isCustomizationItemSupported(/* matchIsCustomLink= */ true)
                        && !mTileDragDelegate.isFirstDraggableTile(mView);
            case ContextMenuItemId.MOVE_DOWN:
                return isCustomizationItemSupported(/* matchIsCustomLink= */ true)
                        && !mTileDragDelegate.isLastDraggableTile(mView);
            default:
                return false;
        }
    }

    @Override
    public boolean hasSpaceForPinnedShortcut() {
        return mCustomTileModificationDelegate.hasSpace();
    }

    @Override
    public void onContextMenuCreated() {}

    @Override
    public void hideAllItems() {}

    // TileDragSession.EventListener implementation.
    @Override
    public void onDragStart() {
        mTileDragDelegate.showDivider(/* isAnimated= */ true);
    }

    @Override
    public void onDragDominate() {
        mContextMenuManager.hideListContextMenu();
    }

    @Override
    public boolean onReorderAccept(
            @ReorderFlow int reorderFlow,
            SiteSuggestion fromSuggestion,
            SiteSuggestion toSuggestion) {
        switch (reorderFlow) {
            case ReorderFlow.DRAG_FLOW:
                RecordUserAction.record("Suggestions.Drag.ReorderItem");
                break;
            case ReorderFlow.SWAP_FLOW:
                RecordUserAction.record("Suggestions.Keyboard.ReorderItem");
                break;
        }
        return mCustomTileModificationDelegate.reorder(
                fromSuggestion,
                toSuggestion,
                () -> {
                    // Refresh has taken place, and the divider is re-rendered. For seamless
                    // transition, show divider immediately, then hide it with animation.
                    mTileDragDelegate.showDivider(/* isAnimated= */ false);
                    mTileDragDelegate.hideDivider(/* isAnimated= */ true);
                });
    }

    @Override
    public void onReorderCancel() {
        mTileDragDelegate.hideDivider(/* isAnimated= */ true);
    }

    boolean isCustomizationEnabledForTileSection() {
        return ChromeFeatureList.sMostVisitedTilesCustomization.isEnabled()
                && mTile.getSectionType() == TileSectionType.PERSONALIZED;
    }

    boolean isCustomLink() {
        return mTile.getSource() == TileSource.CUSTOM_LINKS;
    }

    boolean isCustomizationItemSupported(boolean matchIsCustomLink) {
        if (!isCustomizationEnabledForTileSection()) {
            return false;
        }
        return isCustomLink() == matchIsCustomLink;
    }
}
