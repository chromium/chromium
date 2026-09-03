// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
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
        setId(R.id.tab_sharing_toolbar_container);
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
     * Atomically swaps one toolbar view for another without changing the number of visible
     * toolbars, so the reported browser controls height is unaffected. Used during a tab sharing
     * source switch, where the old session's toolbar is replaced by the new session's toolbar.
     * Swapping in place avoids the height collapsing to zero and back (a separate {@link
     * #removeToolbar} followed by {@link #addToolbar}) which would make the web contents jump.
     *
     * @param oldToolbar The view to remove.
     * @param newToolbar The view to add in its place.
     */
    public void swapToolbar(View oldToolbar, View newToolbar) {
        int index = indexOfChild(oldToolbar);
        if (index != -1) {
            removeViewAt(index);
            addView(newToolbar, index);
        } else {
            addView(newToolbar);
        }
        // Reconcile height/visibility once, after both mutations, so the transient zero-child state
        // is never reported to the top controls stacker.
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
        // Not the bottommost layer, so it must be scrollable; it never actually scrolls off since
        // tab sharing is desktop-only and the top controls are locked there.
        return ScrollBehavior.DEFAULT_SCROLLABLE;
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
