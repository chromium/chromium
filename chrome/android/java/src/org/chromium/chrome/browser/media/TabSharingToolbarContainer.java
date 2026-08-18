// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.chromium.chrome.R;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.TopControlLayer;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.ScrollBehavior;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.ui.base.ViewUtils;

/**
 * Container for tab sharing toolbars in each window.
 *
 * <p>This supports stacking multiple toolbars below the browser top controls when multiple media
 * projection sessions are supported (crbug.com/487666920).
 */
@NullMarked
public class TabSharingToolbarContainer extends LinearLayout implements TopControlLayer {
    private final TopControlsStacker mTopControlsStacker;
    private final int mToolbarHeightPx;
    private int mHeight;

    /**
     * Constructs the container for tab sharing toolbars in each window.
     *
     * @param context The Android context.
     * @param stacker The stacker that manages top controls.
     */
    public TabSharingToolbarContainer(Context context, TopControlsStacker stacker) {
        super(context);
        mTopControlsStacker = stacker;
        mToolbarHeightPx =
                context.getResources().getDimensionPixelSize(R.dimen.tab_sharing_toolbar_height);
        setOrientation(VERTICAL);
        addOnLayoutChangeListener(this::onLayoutChanged);
        updateVisibility();
    }

    /**
     * Updates the container's visibility and synchronously seeds the reported browser controls
     * height from a fixed dimension, based on the number of visible child toolbars. The seed is
     * later reconciled with the actual laid-out height in {@link #onLayoutChanged}.
     */
    private void updateVisibility() {
        // Count visible toolbars rather than assuming a single toolbar. Although the OS currently
        // permits only one active tab sharing session at a time, this container is designed to
        // stack multiple toolbars to support multiple concurrent sessions in the future
        // (crbug.com/487666920).
        int visibleToolbars = 0;
        for (int i = 0; i < getChildCount(); i++) {
            if (getChildAt(i).getVisibility() != View.GONE) {
                visibleToolbars++;
            }
        }
        int expectedHeight = visibleToolbars * mToolbarHeightPx;
        int visibility = visibleToolbars > 0 ? View.VISIBLE : View.GONE;

        boolean visibilityChanged = getVisibility() != visibility;
        if (visibilityChanged) {
            setVisibility(visibility);
        }
        if (mHeight != expectedHeight || visibilityChanged) {
            mHeight = expectedHeight;
            mTopControlsStacker.requestLayerUpdateSync(false);
        }
    }

    /**
     * Cleans up the container by removing all child views and resetting its visibility footprint in
     * the top controls stack.
     */
    public void destroy() {
        removeAllViews();
        updateVisibility();
    }

    /**
     * Adds a tab sharing toolbar view to the container.
     *
     * @param toolbar The view to add.
     */
    public void addToolbar(View toolbar) {
        addView(toolbar);
        updateVisibility();
    }

    /**
     * Removes a tab sharing toolbar view from the container.
     *
     * @param toolbar The view to remove.
     */
    public void removeToolbar(View toolbar) {
        removeView(toolbar);
        updateVisibility();
    }

    /**
     * Reconciles the fixed-dimension height seed with the actual laid-out height, e.g. when the
     * status text wraps to multiple lines or the font scale / display density changes. Transient
     * zero heights (such as for a not-yet-laid-out hidden window) are ignored so the synchronous
     * seed from {@link #updateVisibility} is preserved.
     */
    private void onLayoutChanged(
            View v, int l, int t, int r, int b, int oldL, int oldT, int oldR, int oldB) {
        int newHeight = b - t;
        if (newHeight > 0 && newHeight != mHeight) {
            mHeight = newHeight;
            mTopControlsStacker.requestLayerUpdateSync(false);
        }
    }

    // Implements TopControlLayer

    @Override
    public @TopControlType int getTopControlType() {
        return TopControlType.TAB_SHARING_TOOLBAR;
    }

    @Override
    public int getTopControlHeight() {
        return mHeight;
    }

    @Override
    public @TopControlVisibility int getTopControlVisibility() {
        if (getChildCount() == 0) return TopControlVisibility.HIDDEN;
        for (int i = 0; i < getChildCount(); i++) {
            if (getChildAt(i).getVisibility() != View.GONE) {
                return TopControlVisibility.VISIBLE;
            }
        }
        return TopControlVisibility.HIDDEN;
    }

    @Override
    public @ScrollBehavior int getScrollBehavior() {
        // The tab sharing toolbar is pinned: it stays fixed at the top for the duration of a
        // sharing session and never scrolls off with the web contents. It has no composited
        // (SceneLayer) counterpart and is positioned entirely by the Android view layout via its
        // top margin (see onTopControlLayerHeightChanged), with the web contents inset purely by
        // its reported height, so it needs no per-frame offset handling and relies on
        // TopControlLayer's default no-op onBrowserControlsOffsetUpdate().
        return ScrollBehavior.NEVER_SCROLLABLE;
    }

    @Override
    public void onTopControlLayerHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        if (getTopControlVisibility() == TopControlVisibility.VISIBLE) {
            int topMargin = topControlsHeight - mHeight;
            ViewGroup.LayoutParams lp = getLayoutParams();
            if (lp instanceof ViewGroup.MarginLayoutParams mlp) {
                if (mlp.topMargin != topMargin) {
                    mlp.topMargin = topMargin;
                    ViewUtils.requestLayout(
                            this, "TabSharingToolbarContainer.onTopControlLayerHeightChanged");
                }
            }
        }
    }
}
