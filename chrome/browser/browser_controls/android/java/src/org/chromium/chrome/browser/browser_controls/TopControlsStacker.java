// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import android.util.SparseIntArray;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
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
    public static final int INVALID_HEIGHT = -1;

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
        TopControlVisibility.SHOWING_TOP_ANCHOR,
        TopControlVisibility.SHOWING_BOTTOM_ANCHOR,
        TopControlVisibility.HIDING_TOP_ANCHOR,
        TopControlVisibility.HIDING_BOTTOM_ANCHOR
    })
    public @interface TopControlVisibility {
        int VISIBLE = 0;
        int HIDDEN = 1;

        /**
         * The current layer is going through animation from HIDDEN to VISIBLE. Layers below will
         * move downwards until this layer is fully shown.
         */
        int SHOWING_TOP_ANCHOR = 3;

        /**
         * The current layer is going through animation from HIDDEN to VISIBLE. The layer will move
         * downwards with layers below until fully shown.
         */
        int SHOWING_BOTTOM_ANCHOR = 4;

        /**
         * The current layer is going through animation from VISIBLE to HIDDEN. Layers below will
         * shift upwards until fully cover the current layer.
         */
        int HIDING_TOP_ANCHOR = 5;

        /**
         * The current layer is going through animation from VISIBLE to HIDDEN. The layer will shift
         * upwards until fully cover by the top layer
         */
        int HIDING_BOTTOM_ANCHOR = 6;
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
    private final SparseIntArray mLayerRestingOffsets = new SparseIntArray();
    private final SparseIntArray mLayerYOffset = new SparseIntArray();

    private final BrowserControlsSizer mBrowserControlsSizer;
    private final BrowserControlsVisibilityDelegate mBrowserControlsVisibilityDelegate;
    private final Callback<@BrowserControlsState Integer> mBrowserControlsStateCallback =
            this::updateBrowserControlsState;
    private @BrowserControlsState int mBrowserControlsState = BrowserControlsState.BOTH;

    private boolean mScrollingDisabled;

    private int mTotalHeight;
    private int mMinHeight;
    private @Nullable BrowserControlsOffsetTagsInfo mTopControlsOffsetTagInfo;
    private boolean mIsMinHeightShrinking;

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
     * Trigger the browser controls height update based on the current layer status. If there's
     * already an animated transition running, this call might cause it to skip to the end state.
     *
     * @param animate Whether animate the browser controls size change.
     */
    public void requestLayerUpdate(boolean animate) {
        if (!ChromeFeatureList.sTopControlsRefactor.isEnabled()) return;

        recalculateHeights();
        recalculateLayerRestingOffsets();
        updateTopControlsHeight(animate);

        // When reposition happening when browser controls is overriding offsets, we need to
        // reposition immediately.
        if (mBrowserControlsSizer.offsetOverridden()) {
            repositionLayers(
                    mBrowserControlsSizer.getTopControlOffset(),
                    mBrowserControlsSizer.getTopControlsMinHeightOffset(),
                    animate,
                    isBrowserControlsVisibilityForced());
        }

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

            if (!isLayerHidden(layer)) {
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
            if (isLayerHidden(layer)) continue;
            if (!layer.contributesToTotalHeight() || isLayerHiding(layer)) continue;

            totalHeight += layer.getTopControlHeight();

            boolean hasMinHeight = doesLayerHasMinHeight(layer);
            if (hasMinHeight) {
                minHeight += layer.getTopControlHeight();

                assert minHeight == totalHeight
                        : "All layers with minHeight should be added before a scrollable layer.";
            }

            if (ChromeFeatureList.sBrowserControlsInViz.isEnabled()) {
                layer.updateOffsetTag(hasMinHeight ? null : mTopControlsOffsetTagInfo);
            }
        }
        mTotalHeight = totalHeight;
        mMinHeight = minHeight;
    }

    // Calculate the layer's resting offsets assuming the top control is fully shown.
    private void recalculateLayerRestingOffsets() {
        int cumulativeHeight = 0;
        for (@TopControlType int type : STACK_ORDER) {
            TopControlLayer layer = mControls.get(type);
            if (layer == null) continue;

            @TopControlVisibility int layerVisibility = layer.getTopControlVisibility();
            if (layerVisibility == TopControlVisibility.HIDDEN
                    || layerVisibility == TopControlVisibility.HIDING_TOP_ANCHOR
                    || layerVisibility == TopControlVisibility.HIDING_BOTTOM_ANCHOR) {
                // If a layer is hidden / hiding, it does not have a resting offset.
                mLayerRestingOffsets.delete(type);
            } else {
                mLayerRestingOffsets.put(type, cumulativeHeight);
                if (layer.contributesToTotalHeight()) {
                    cumulativeHeight += layer.getTopControlHeight();
                }
            }
        }
    }

    // Core logic to dispatch offset to top control layers. This handles offsets either during user
    // scrolling, or a browser or render driven animation is ran.
    private void repositionLayers(
            int initialTopOffset,
            int initialTopControlsMinHeightOffset,
            boolean requestNewFrame,
            boolean offsetsAppliedByBrowser) {
        if (!ChromeFeatureList.sTopControlsRefactor.isEnabled()
                || !ChromeFeatureList.sTopControlsRefactorV2.isEnabled()) return;

        // 1. Calculate the offset based on the current layer position. In this step, the controls
        // are classified into scrollable and non-scrollable layer, and all the layers are display
        // at its full height.
        SparseIntArray yOffsetOfLayers = new SparseIntArray();
        if (ChromeFeatureList.sBrowserControlsInViz.isEnabled() && !offsetsAppliedByBrowser) {
            // If offset can be handled by render, put layers at their resting positions.
            for (@TopControlType int type : STACK_ORDER) {
                TopControlLayer layer = mControls.get(type);
                if (isLayerHidden(layer)) {
                    continue;
                }

                yOffsetOfLayers.put(type, mLayerRestingOffsets.get(type));
            }
        } else {
            calculateStackLayersOffsets(
                    yOffsetOfLayers, initialTopOffset, initialTopControlsMinHeightOffset);
        }

        // 2. Adjustments. The previous step assumes all the layers are display in full height at
        // resting state. When animating size changes, one or more layer(s) could be in its
        // showing / hiding phase, and the other layers needs to shift accordingly.
        //
        // Compare and fix the yOffset with the previous mLayerOffsets if reposition
        // is caused by an animated browser controls height adjustment. This needs to run in a
        // different loop to cooperate browser controls height reduction, as we need to still push
        // updates to layer that's changed from visible -> hidden.
        boolean hasAnimatingLayer = false;
        for (@TopControlType int type : STACK_ORDER) {
            TopControlLayer layer = mControls.get(type);
            if (layer == null) continue;
            hasAnimatingLayer =
                    layer.getTopControlVisibility() != TopControlVisibility.VISIBLE
                            && layer.getTopControlVisibility() != TopControlVisibility.HIDDEN;
            if (hasAnimatingLayer) {
                break;
            }
        }

        // The algorithm adjust layer's offset based on whether the control is showing or hiding.
        if (requestNewFrame && initialTopOffset != 0 && hasAnimatingLayer) {
            // When animated size change, the browser controls will try to ensure it snaps to its
            // resting position. We'll use the minHeight as comparison first, then compare the
            // topOffset.
            boolean isShrinking;

            if (initialTopControlsMinHeightOffset != mMinHeight) {
                isShrinking = initialTopControlsMinHeightOffset > mMinHeight;
                mIsMinHeightShrinking = isShrinking;
            } else {
                // At the last frame of minHeight shrinking (initialTopControlsMinHeightOffset ==
                // mMinHeight) while browser controls is scrolled off (thus initialTopOffset < 0),
                // we need to manually correct the |isShrinking| so the layer adjustment below
                // is towards the correct direction. We keep this as mIsMinHeightShrinking so it
                // remembers the minHeight movement direction from the previous frame.
                isShrinking = mIsMinHeightShrinking || (initialTopOffset > 0);
                mIsMinHeightShrinking = false;
            }

            // adjustedYOffset represents the the expected start for the current layer. The default
            // is 0 so if the first layer is hiding, it can be used as a fallback value.
            int adjustedYOffset = 0;
            for (@TopControlType int type : STACK_ORDER) {
                TopControlLayer layer = mControls.get(type);
                if (isLayerHidden(layer)) continue;

                if (layer.getTopControlVisibility() == TopControlVisibility.HIDING_BOTTOM_ANCHOR) {
                    // If the current layer is hiding in progress with bottom anchor, it will be
                    // disconnected with the top layer's bottom. As on initialTopOffset approach
                    // to 0, it will trend towards the layer's full height.
                    // NOTE: This does not support adjustment for more than one layer.
                    int accumulatedMovements = 0;
                    if (doesLayerHasMinHeight(layer)) {
                        accumulatedMovements =
                                Math.max(
                                        0,
                                        layer.getTopControlHeight()
                                                - initialTopControlsMinHeightOffset);
                    } else {
                        accumulatedMovements =
                                Math.max(0, layer.getTopControlHeight() - initialTopOffset);
                    }
                    adjustedYOffset = adjustedYOffset - accumulatedMovements;
                } else if (layer.getTopControlVisibility()
                        == TopControlVisibility.SHOWING_TOP_ANCHOR) {
                    // When the layer is top-anchored, we adjust its position to resting offset
                    // as soon as it is ready to be shown.
                    int restingOffsets = mLayerRestingOffsets.get(type);
                    if (adjustedYOffset < restingOffsets
                            && adjustedYOffset + layer.getTopControlHeight() >= restingOffsets) {
                        adjustedYOffset = restingOffsets;
                    }
                } else {
                    adjustedYOffset = yOffsetOfLayers.get(type, adjustedYOffset);
                }

                // If layer does not have a previousYOffset, meaning this is the first time
                // it is getting repositioned. When the layer is showing bottom anchor, we need to
                // adjust the layer up so its bottom it's connect to the next layer.
                int previousYOffset;
                if (layer.getTopControlVisibility() == TopControlVisibility.SHOWING_BOTTOM_ANCHOR) {
                    previousYOffset = adjustedYOffset - layer.getTopControlHeight();
                } else {
                    previousYOffset = mLayerYOffset.get(type, adjustedYOffset);
                }

                if (isShrinking) {
                    // When layers are shrinking, none of the layers should move downwards.
                    // (e.g. yOffset should decrease)
                    adjustedYOffset = Math.min(adjustedYOffset, previousYOffset);
                } else {
                    // When browser controls growing, none of the layers should move upwards.
                    // (e.g. yOffset should increase)
                    adjustedYOffset = Math.max(adjustedYOffset, previousYOffset);
                }
                yOffsetOfLayers.put(type, adjustedYOffset);

                // Increase the layerYOffset, so it represents the bottom of the current layer
                // as well as the expect starting point for the next layer.
                adjustedYOffset += layer.getTopControlHeight();
            }
        }

        // 3. Dispatch yOffset to layers after cleanup, also tells layer to clean up its state
        //   when they are at resting.
        boolean controlsAtResting =
                initialTopOffset == 0 || initialTopOffset == mMinHeight - mTotalHeight;
        for (int type : STACK_ORDER) {
            TopControlLayer layer = mControls.get(type);
            if (layer == null) continue;

            // Save the offset for future animation use.
            int yOffset = yOffsetOfLayers.get(type, -layer.getTopControlHeight());
            if (layer.getTopControlVisibility() == TopControlVisibility.HIDDEN) {
                mLayerYOffset.delete(type);
            } else {
                mLayerYOffset.put(type, yOffset);
            }

            layer.onBrowserControlsOffsetUpdate(yOffset, controlsAtResting);
        }
    }

    // Calculate the offset based on stack order.
    private void calculateStackLayersOffsets(
            SparseIntArray yOffsetOfLayers, int topControlsOffset, int topControlsMinHeightOffset) {

        int validationHeight = 0;
        int validationMinHeight = 0;

        // Limit the topControlsMinHeightOffset to mMinHeight, similar to bottom controls.
        // (See crbug.com/359539294). Then, convert the minHeightOffsets (resting at |minHeight|) to
        // be the same coordinates as topOffset (resting at 0).
        // When minHeight is increasing (in animation), this value should be negative value, similar
        // to top controls; when minHeight decreases, the nonScrollableYOffset is a positive value.
        int nonScrollableYOffset = Math.min(topControlsMinHeightOffset, mMinHeight) - mMinHeight;
        int scrollableYOffset = topControlsOffset;

        for (@TopControlType int type : STACK_ORDER) {
            TopControlLayer layer = mControls.get(type);
            if (isLayerHidden(layer)) continue;

            // If a layer is hiding, skip the layer, as the layer do not exists in the final
            // stack order.
            if (isLayerHiding(layer)) continue;

            boolean hasMinHeight = doesLayerHasMinHeight(layer);
            int layerHeight = layer.contributesToTotalHeight() ? layer.getTopControlHeight() : 0;

            validationHeight += layerHeight;
            validationMinHeight += hasMinHeight ? layerHeight : 0;

            if (hasMinHeight) {
                // First portion: Non-scrollable layers.
                yOffsetOfLayers.put(type, nonScrollableYOffset);
                nonScrollableYOffset += layerHeight;

                // As we are still calculating the offset for non-scrollable layers, the first
                // scrollable layer's baseline should be increased too, but not at the point where
                // it would exceed the nonScrollableYOffset.
                scrollableYOffset = Math.min(nonScrollableYOffset, scrollableYOffset + layerHeight);
            } else {
                // Second portion: Scrollable layers.
                // To avoid scrollable layers keeps getting update after it is scrolled off,
                // limit the yOffset, so the scrollable layer's bottom is aligned with
                // the bottom of the last non-scrollable layer (nonScrollableYOffset).
                int optimizedYOffset =
                        Math.max(scrollableYOffset, nonScrollableYOffset - layerHeight);
                yOffsetOfLayers.put(type, optimizedYOffset);
                scrollableYOffset += layerHeight;
            }
        }

        logIfHeightMismatch(mTotalHeight, mMinHeight, validationHeight, validationMinHeight);
    }

    private boolean doesLayerHasMinHeight(TopControlLayer layer) {
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

    @Contract("null -> true")
    private static boolean isLayerHidden(@Nullable TopControlLayer layer) {
        return layer == null || layer.getTopControlVisibility() == TopControlVisibility.HIDDEN;
    }

    private static boolean isLayerHiding(TopControlLayer layer) {
        @TopControlVisibility int visibility = layer.getTopControlVisibility();
        return visibility == TopControlVisibility.HIDING_TOP_ANCHOR
                || visibility == TopControlVisibility.HIDING_BOTTOM_ANCHOR;
    }

    private boolean isBrowserControlsVisibilityForced() {
        return mBrowserControlsState == BrowserControlsState.HIDDEN
                || mBrowserControlsState == BrowserControlsState.SHOWN;
    }

    /**
     * Calculates the total height of the UI from the specified layer to the top of the screen.
     *
     * <p>This method computes the cumulative height of all visible layers starting from the top
     * most layer until the specified layer **(exclusive)**.
     *
     * <p><b>Warning:</b> The height returned might not be accurate during {@link
     * #repositionLayers(int, int, boolean, boolean)} ()}, so it should not be used to determine a
     * layer's attribute.
     *
     * @param stopLayer the layer in the stack order to stop at.
     * @return the total height of the visible UI from the specified layer to the top, or {@link
     *     #INVALID_HEIGHT} if the layer type is invalid.
     */
    public int getHeightFromLayerToTop(@TopControlType int stopLayer) {
        int height = 0;
        for (@TopControlType int type : STACK_ORDER) {
            TopControlLayer layer = mControls.get(type);

            if (type == stopLayer) {
                return height;
            } else if (layer != null) {
                height += layer.getTopControlHeight();
            }
        }

        return INVALID_HEIGHT;
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
        if (!ChromeFeatureList.sTopControlsRefactor.isEnabled()) return;

        if (mTopControlsOffsetTagInfo == offsetTagsInfo && mBrowserControlsState == constraints) {
            return;
        }
        mTopControlsOffsetTagInfo = offsetTagsInfo;
        mBrowserControlsState = constraints;
        requestLayerUpdate(false);
    }

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            boolean topControlsMinHeightChanged,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean bottomControlsMinHeightChanged,
            boolean requestNewFrame,
            boolean isVisibilityForced) {
        if (mControls.isEmpty()) return;

        repositionLayers(
                topOffset,
                topControlsMinHeightOffset,
                requestNewFrame,
                requestNewFrame || isVisibilityForced);
    }

    /** Tear down |this| and clear all existing controls from the Map. */
    public void destroy() {
        mControls.clear();
        mBrowserControlsVisibilityDelegate.removeObserver(mBrowserControlsStateCallback);
        mBrowserControlsSizer.removeObserver(this);
    }

    private static void logIfHeightMismatch(
            int expectedHeight, int expectedMinHeight, int actualHeight, int actualMinHeight) {

        if (expectedHeight == actualHeight && expectedMinHeight == actualMinHeight) return;

        Log.w(
                TAG,
                "Height mismatch observed."
                        + " [Expected]"
                        + " expectedHeight= "
                        + expectedHeight
                        + " expectedMinHeight= "
                        + expectedMinHeight
                        + " [Actual]"
                        + " actualHeight = "
                        + actualHeight
                        + " actualMinHeight= "
                        + actualMinHeight);
    }
}
