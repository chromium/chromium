// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.cc.input.OffsetTag;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.HashMap;
import java.util.Map;

/**
 * Coordinator class for UI layers in the top browser controls. This class manages the relative
 * y-axis position for every registered top control layer.
 */
@NullMarked
public class TopControlsStacker implements BrowserControlsStateProvider.Observer {
    private static final String TAG = "TopControlsStacker";

    /** Enums that defines the types of top controls. */
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        TopControlType.STATUS_INDICATOR,
        TopControlType.TABSTRIP,
        TopControlType.TOOLBAR,
        TopControlType.BOOKMARK_BAR,
        TopControlType.HAIRLINE,
        TopControlType.PROGRESS_BAR,
    })
    public @interface TopControlType {
        int STATUS_INDICATOR = 0;
        int TABSTRIP = 1;
        int TOOLBAR = 2;
        int BOOKMARK_BAR = 3;
        int HAIRLINE = 4;
        int PROGRESS_BAR = 5;
    }

    /** Enum that defines the possible visibilities of a top control. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        TopControlVisibility.VISIBLE,
        TopControlVisibility.HIDDEN,
    })
    public @interface TopControlVisibility {
        int VISIBLE = 0;
        int HIDDEN = 1;
    }

    /** Enum that defines the scroll behavior of a top control. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        ScrollBehavior.DEFAULT_SCROLLABLE,
        ScrollBehavior.NEVER_SCROLLABLE,
    })
    public @interface ScrollBehavior {
        int DEFAULT_SCROLLABLE = 0;
        int NEVER_SCROLLABLE = 1;
    }

    // The pre-defined stack order for different top controls.
    private static final @TopControlType int[] STACK_ORDER =
            new int[] {
                TopControlType.STATUS_INDICATOR,
                TopControlType.TABSTRIP,
                TopControlType.TOOLBAR,
                TopControlType.BOOKMARK_BAR,
                TopControlType.HAIRLINE,
                TopControlType.PROGRESS_BAR,
            };

    // All controls are stored in a Map and we should only have one of each control type.
    private final Map<@TopControlType Integer, TopControlLayer> mControls;

    private final BrowserControlsSizer mBrowserControlsSizer;
    private final BrowserControlsVisibilityDelegate mBrowserControlsVisibilityDelegate;
    private final Callback<@BrowserControlsState Integer> mBrowserControlsStateCallback =
            this::updateBrowserControlsState;
    private @BrowserControlsState int mBrowserControlsState = BrowserControlsState.BOTH;

    private boolean mScrollingDisabled;

    private int mTotalHeight;
    private int mMinHeight;

    private @Nullable OffsetTag mTopControlsOffsetTag;

    /**
     * Constructs the top controls stacker, which is used to calculate heights and offsets for any
     * top controls.
     *
     * @param browserControlsSizer {@link BrowserControlsSizer} to request browser controls changes.
     */
    public TopControlsStacker(BrowserControlsSizer browserControlsSizer) {
        mControls = new HashMap<>();
        mBrowserControlsSizer = browserControlsSizer;
        mBrowserControlsVisibilityDelegate = mBrowserControlsSizer.getBrowserVisibilityDelegate();

        mBrowserControlsSizer.addObserver(this);
        mBrowserControlsVisibilityDelegate.addObserver(mBrowserControlsStateCallback);
    }

    /**
     * Adds a new control layer to the list of active top controls. Note that the control's height
     * will not be recalculated until {@link #requestLayerUpdate(boolean)} is called.
     *
     * @param newControl TopControlLayer to add to the active controls.
     */
    public void addControl(TopControlLayer newControl) {
        assert mControls.get(newControl.getTopControlType()) == null
                : "Trying to add a duplicate control type.";
        mControls.put(newControl.getTopControlType(), newControl);
    }

    /**
     * Removes a control layer from the list of active top controls. Note that the control's height
     * will not be recalculated until {@link #requestLayerUpdate(boolean)} is called.
     *
     * @param control The TopControlLayer to remove from the active controls.
     */
    public void removeControl(TopControlLayer control) {
        mControls.remove(control.getTopControlType());
    }

    /**
     * Sets whether scrolling is disabled for the top controls.
     *
     * @param disabled Whether scrolling is disabled.
     */
    public void setScrollingDisabled(boolean disabled) {
        if (mScrollingDisabled == disabled) return;
        mScrollingDisabled = disabled;

        // This call can potentially still change the browser control's shown ration when minHeight
        // is updated when the controls is scrolled off, or when BrowserControlsState.HIDDEN.
        // We intentionally disabling animation updates for those.
        requestLayerUpdate(false);
    }

    /**
     * Returns the total height of all currently visible {@link TopControlLayer} controls of this
     * instance that also contribute to the total height of the controls.
     *
     * @return The total height of all visible controls in pixels.
     */
    public int getVisibleTopControlsTotalHeight() {
        return mTotalHeight;
    }

    /**
     * Returns the min height of all currently visible {@link TopControlLayer} controls of this
     * instance.
     *
     * @return The min height of all visible controls in pixels.
     */
    public int getVisibleTopControlsMinHeight() {
        return mMinHeight;
    }

    /**
     * Returns the current OffsetTag for the top controls provided by the {@link
     * BrowserControlsStateProvider.Observer}.
     *
     * @return The OffsetTag for the top controls.
     */
    public @Nullable OffsetTag getTopControlsOffsetTag() {
        return mTopControlsOffsetTag;
    }

    /**
     * Trigger the browser controls height update based on the current layer status. If there's
     * already an animated transition running, this call might cause it to skip to the end state.
     *
     * @param animate Whether animate the browser controls size change.
     */
    public void requestLayerUpdate(boolean animate) {
        recalculateHeights();
        updateTopControlsHeight(animate);

        // Add more implementations here when necessary (e.g. offset calculation)
    }

    /**
     * Returns true when the given control type is at the bottom of the set of top controls. We
     * define the bottom as the point in the stack that has no non-null, visible,
     * height-contributing layers beyond it.
     *
     * @param controlType Type of control to query for.
     * @return Whether or not the control is at the bottom.
     */
    public boolean isLayerAtBottom(@TopControlType int controlType) {
        // A null layer (not in the map) cannot be at the bottom.
        if (mControls.get(controlType) == null) return false;

        // Find the bottom-most visible layer that contributes to the total height of the top
        // controls (i.e. the first we encounter). If it is the same as the given |controlType|,
        // then that type is the bottom layer.
        for (int i = STACK_ORDER.length - 1; i >= 0; i--) {
            @TopControlType int currentType = STACK_ORDER[i];
            TopControlLayer layer = mControls.get(currentType);

            if (layer != null && layer.getTopControlVisibility() == TopControlVisibility.VISIBLE) {
                return currentType == controlType;
            }
        }

        // No visible, height-contributing layers were found, so this layer cannot be the bottom.
        return false;
    }

    private void recalculateHeights() {
        int totalHeight = 0;
        int minHeight = 0;
        for (@TopControlType int type : STACK_ORDER) {
            TopControlLayer layer = mControls.get(type);
            if (layer == null || !layer.contributesToTotalHeight()) continue;
            if (layer.getTopControlVisibility() != TopControlVisibility.VISIBLE) continue;

            totalHeight += layer.getTopControlHeight();
            if (isLayerAlwaysVisible(layer)) {
                minHeight += layer.getTopControlHeight();
            }
        }
        mTotalHeight = totalHeight;
        mMinHeight = minHeight;
    }

    private boolean isLayerAlwaysVisible(TopControlLayer layer) {
        if (layer.getScrollBehavior() == ScrollBehavior.NEVER_SCROLLABLE) {
            return true;
        }

        if (mScrollingDisabled) {
            return mBrowserControlsState == BrowserControlsState.SHOWN
                    || mBrowserControlsState == BrowserControlsState.BOTH;
        }

        return false;
    }

    private void updateTopControlsHeight(boolean requireAnimations) {
        if (requireAnimations) {
            mBrowserControlsSizer.setAnimateBrowserControlsHeightChanges(true);
        }
        mBrowserControlsSizer.setTopControlsHeight(mTotalHeight, mMinHeight);
        if (requireAnimations) {
            mBrowserControlsSizer.setAnimateBrowserControlsHeightChanges(false);
        }
    }

    private void updateBrowserControlsState(@BrowserControlsState int newState) {
        if (mBrowserControlsState == newState) return;
        mBrowserControlsState = newState;
        if (mScrollingDisabled) {
            requestLayerUpdate(false);
        }
    }

    // BrowserControlsStateProvider.Observer implementation:

    @Override
    public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        // No-op by default until refactor work is enabled.
        if (!ChromeFeatureList.sTopControlsRefactor.isEnabled()) return;

        // Inform any controls that there was a change to the top controls height.
        for (TopControlLayer topControlLayer : mControls.values()) {
            topControlLayer.onTopControlLayerHeightChanged(topControlsHeight, topControlsMinHeight);
        }
    }

    @Override
    public void onOffsetTagsInfoChanged(
            BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
            BrowserControlsOffsetTagsInfo offsetTagsInfo,
            @BrowserControlsState int constraints,
            boolean shouldUpdateOffsets) {
        // TODO(crbug.com/417238089): Consider pushing updated OffsetTags to TopControlLayers.
        if (mTopControlsOffsetTag == offsetTagsInfo.getTopControlsOffsetTag()
                && mBrowserControlsState == constraints) {
            return;
        }
        mTopControlsOffsetTag = offsetTagsInfo.getTopControlsOffsetTag();
        mBrowserControlsState = constraints;
        if (mScrollingDisabled) {
            requestLayerUpdate(false);
        }
    }

    /** Tear down |this| and clear all existing controls from the Map. */
    public void destroy() {
        mControls.clear();
        mBrowserControlsVisibilityDelegate.removeObserver(mBrowserControlsStateCallback);
        mBrowserControlsSizer.removeObserver(this);
    }
}
