// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import android.util.SparseArray;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Coordinator class for UI layers in the bottom browser controls. This class manages the relative
 * y-axis position for every registered bottom control elements, and their background colors.
 */
public class BottomControlsStacker implements BrowserControlsStateProvider.Observer {
    private static final String TAG = "BotControlsStacker";
    private static final int INVALID_HEIGHT = -1;
    private static boolean sDumpLayerUpdateForTesting;

    /** Enums that defines the type and position for each bottom controls. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        LayerType.TABSTRIP_TOOLBAR,
        LayerType.READ_ALOUD_PLAYER,
        LayerType.BOTTOM_TOOLBAR,
        LayerType.BOTTOM_CHIN,
        LayerType.TEST_BOTTOM_LAYER
    })
    public @interface LayerType {
        int TABSTRIP_TOOLBAR = 0;
        int READ_ALOUD_PLAYER = 1;
        int BOTTOM_TOOLBAR = 2;
        int BOTTOM_CHIN = 3;

        // Layer that's used for testing.
        int TEST_BOTTOM_LAYER = 100;
    }

    /** Enums that defines the scroll behavior for different controls. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
        LayerScrollBehavior.NEVER_SCROLL_OFF,
        LayerScrollBehavior.DEFAULT_SCROLL_OFF
    })
    public @interface LayerScrollBehavior {
        int ALWAYS_SCROLL_OFF = 0;
        int NEVER_SCROLL_OFF = 1;

        /**
         * By default, this layer will scroll off. However, if this layer is positioned underneath a
         * visible layer that is NEVER_SCROLL_OFF, this layer will no longer scroll off.
         */
        int DEFAULT_SCROLL_OFF = 2;
    }

    /** Enums that defines the type and position for each bottom controls. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        LayerVisibility.VISIBLE,
        LayerVisibility.HIDDEN,
        LayerVisibility.SHOWING,
        LayerVisibility.HIDING,
        LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE
    })
    public @interface LayerVisibility {
        int VISIBLE = 0;
        int HIDDEN = 1;

        /**
         * The layer is currently animating to become visible. Its height will contribute to the
         * total height of the bottom controls.
         */
        int SHOWING = 2;

        /**
         * The layer is currently animating to become hidden. Its height will not contribute to the
         * total height of the bottom controls, but its #getHeight should remain at a meaningful
         * value.
         */
        int HIDING = 3;

        /** Will be shown if and only if another layer is labeled as VISIBLE or SHOWING. */
        int VISIBLE_IF_OTHERS_VISIBLE = 4;
    }

    // The pre-defined stack order for different bottom controls.
    private static final @LayerType int[] STACK_ORDER =
            new int[] {
                LayerType.TABSTRIP_TOOLBAR,
                LayerType.READ_ALOUD_PLAYER,
                LayerType.BOTTOM_TOOLBAR,
                LayerType.BOTTOM_CHIN,
                LayerType.TEST_BOTTOM_LAYER
            };

    private final SparseArray<BottomControlsLayer> mLayers = new SparseArray<>();
    // Recorded the yOffset for all current layers. This only record the yOffset for visible layers.
    private final SparseIntArray mLayerYOffsets = new SparseIntArray();
    private final SparseBooleanArray mLayerVisibilities = new SparseBooleanArray();
    private final BrowserControlsSizer mBrowserControlsSizer;

    private int mTotalHeight = INVALID_HEIGHT;
    private int mTotalMinHeight = INVALID_HEIGHT;
    private int mTotalHeightFromSetter = INVALID_HEIGHT;
    private int mTotalMinHeightFromSetter = INVALID_HEIGHT;

    /**
     * Construct the coordination class that's used to position different UIs into the bottom
     * controls.
     *
     * @param browserControlsSizer {@link BrowserControlsSizer} to request browser controls changes.
     */
    public BottomControlsStacker(BrowserControlsSizer browserControlsSizer) {
        mBrowserControlsSizer = browserControlsSizer;
        mBrowserControlsSizer.addObserver(this);
    }

    /**
     * Register a layer into the bottom controls. This does not trigger an immediate reposition to
     * the controls; it's the client's responsibility to manually call {@link #requestLayerUpdate}
     * when the layer is ready.
     */
    public void addLayer(BottomControlsLayer layer) {
        assert layer != null && mLayers.get(layer.getType()) == null
                : "Try to set duplicate layer.";
        mLayers.set(layer.getType(), layer);
    }

    /**
     * Remove the layer. Similar to {@link #addLayer(BottomControlsLayer)}, this does not trigger an
     * immediate reposition to the controls until a client calls {@link #requestLayerUpdate}.
     */
    public void removeLayer(BottomControlsLayer layer) {
        mLayers.remove(layer.getType());
    }

    /**
     * Checks whether there are any layers that are currently visible besides the specified type.
     */
    public boolean hasVisibleLayersOtherThan(@LayerType int typeToExclude) {
        for (int layerType : STACK_ORDER) {
            if (typeToExclude == layerType) continue;

            if (mLayerVisibilities.get(layerType)) return true;
        }
        return false;
    }

    /**
     * Trigger the browser controls height update based on the current layer status. If there's
     * already an animated transition running, this call might cause it to skip to the end state.
     *
     * @param animate Whether animate the browser controls size change.
     */
    public void requestLayerUpdate(boolean animate) {
        assert isEnabled();

        updateLayerVisibilities();
        recalculateLayerSizes();
        updateBrowserControlsHeight(animate);
        if (mBrowserControlsSizer.offsetOverridden() && isDispatchingYOffset()) {
            repositionLayers(
                    mBrowserControlsSizer.getBottomControlOffset(),
                    mBrowserControlsSizer.getBottomControlsMinHeightOffset(),
                    animate);
        }
    }

    private void updateBrowserControlsHeight(boolean animate) {
        if (animate) {
            mBrowserControlsSizer.setAnimateBrowserControlsHeightChanges(true);
        }
        mBrowserControlsSizer.setBottomControlsHeight(mTotalHeight, mTotalMinHeight);
        if (animate) {
            mBrowserControlsSizer.setAnimateBrowserControlsHeightChanges(false);
        }
    }

    /**
     * @return {@link BrowserControlsStateProvider} instance in the current Activity.
     */
    public BrowserControlsStateProvider getBrowserControls() {
        return mBrowserControlsSizer;
    }

    /**
     * Note: New callers should just use #requestLayerUpdate directly.
     *
     * <p>Request update the bottom controls height. Internally, the call is routed to the inner
     * {@link BrowserControlsSizer}.
     *
     * @param height The new height for the bottom browser controls
     * @param minHeight The new min height for the bottom browser controls.
     * @param animate Whether the height change required to be animated.
     * @see BrowserControlsSizer#setBottomControlsHeight(int, int)
     * @see BrowserControlsSizer#setAnimateBrowserControlsHeightChanges(boolean)
     */
    public void setBottomControlsHeight(int height, int minHeight, boolean animate) {
        mTotalHeightFromSetter = height;
        mTotalMinHeightFromSetter = minHeight;

        if (!isEnabled()) {
            mBrowserControlsSizer.setBottomControlsHeight(height, minHeight);
        } else {
            requestLayerUpdate(animate);
            // Verify the height and min height match the layer setup.
            logIfHeightMismatch(
                    /* expected= */ "HeightFromSetter",
                    mTotalHeightFromSetter,
                    mTotalMinHeightFromSetter,
                    /* actual= */ "LayerHeightCalc",
                    mTotalHeight,
                    mTotalMinHeight);
        }
    }

    /**
     * @see BrowserControlsSizer#notifyBackgroundColor(int).
     */
    public void notifyBackgroundColor(@ColorInt int color) {
        // TODO(crbug.com/345488108): Handle #notifyBackgroundColor in this class.
        mBrowserControlsSizer.notifyBackgroundColor(color);
    }

    /** Destroy this instance and release the dependencies over the browser controls. */
    public void destroy() {
        mLayers.clear();
        mBrowserControlsSizer.removeObserver(this);
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        // Use warning instead of assert, as there are still use cases that's referenced
        // from custom tabs.
        logIfHeightMismatch(
                /* expected= */ "HeightFromSetter",
                mTotalHeightFromSetter,
                mTotalMinHeightFromSetter,
                /* actual= */ "onBottomControlsHeightChanged",
                bottomControlsHeight,
                bottomControlsMinHeight);

        // Verification when we are using layers.
        if (isEnabled()) {
            logIfHeightMismatch(
                    /* expected= */ "LayerHeightCalc",
                    mTotalHeight,
                    mTotalMinHeight,
                    /* actual= */ "onBottomControlsHeightChanged",
                    bottomControlsHeight,
                    bottomControlsMinHeight);

            // If animations are enabled, calls to #onControlsOffsetChanged will reposition the
            // layers. If animations aren't enabled, no such calls will occur, and #repositionLayers
            // should be triggered here.
            if (isDispatchingYOffset()
                    && !mBrowserControlsSizer.shouldAnimateBrowserControlsHeightChanges()) {
                repositionLayers(
                        mBrowserControlsSizer.getBottomControlOffset(),
                        mBrowserControlsSizer.getBottomControlsMinHeightOffset(),
                        false);
            }
        }
    }

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean needsAnimate,
            boolean isVisibilityForced) {
        if (mLayers.size() == 0 || !isDispatchingYOffset()) return;
        repositionLayers(bottomOffset, bottomControlsMinHeightOffset, needsAnimate);
    }

    /** Reposition the layers given that the height and minHeight is known. */
    private void repositionLayers(
            int bottomOffset, int bottomControlsMinHeightOffset, boolean animated) {

        // 0. Initialize the offset for each layer.
        SparseIntArray yOffsetOfLayers = new SparseIntArray(STACK_ORDER.length);
        int height = 0;
        int totalMinHeight = 0;
        int layerBottomOffset = bottomOffset;

        // TODO(crbug.com/359539294) This may not be needed after fixing and re-enabling animations.
        // Some calls right after a height change will have a bottomControlsMinHeightOffset
        // representing an outdated min height. Limit the min height offset to be less than or equal
        // to the total min height to avoid a gap under the bottom controls after a height change.
        // This only seems to have negative effects when reducing the min height (i.e. shrinking
        // the bottom controls).
        bottomControlsMinHeightOffset = Math.min(bottomControlsMinHeightOffset, mTotalMinHeight);

        // Convert the minHeight to use the same axis as bottomOffset (0 as the top of the browser
        // controls; mTotalHeight as the bottom of the bottom controls)
        int minHeightBottomOffset = mTotalHeight - bottomControlsMinHeightOffset;

        // Calculate the height for each layer. Given we have limited number of layers, looping
        // through layers shouldn't be too costly.
        for (int type : STACK_ORDER) {
            BottomControlsLayer layer = mLayers.get(type);
            if (layer == null || !mLayerVisibilities.get(type)) continue;

            boolean shouldScrollOff = shouldLayerScrollOff(layer, totalMinHeight);
            assert totalMinHeight == 0 || !shouldScrollOff
                    : "A scroll-off layer under a NEVER_SCROLL_OFF layer is not supported. Layer: "
                            + layer.getType();

            // 1. Accumulate the layer's height to ensure the height does not change during layout
            // update. This is only used for assertion.
            height += layer.getHeight();
            totalMinHeight += shouldScrollOff ? 0 : layer.getHeight();

            int layerYOffset;
            if (shouldScrollOff) {
                // [Scrollable layers]
                // Increase the layerBottomOffset so it represents the bottomOffset from the bottom
                // edge of the layer. The bottom edge of this layer can sit lower in the controls
                // than the next layer's top edge if the next layer does not scroll off, so set the
                // minValue from the minHeightBottomOffset;
                layerBottomOffset += layer.getHeight();
                layerYOffset = layerBottomOffset - mTotalHeight;

                layerBottomOffset = Math.min(layerBottomOffset, minHeightBottomOffset);
            } else {
                // [Non scrollable layers]
                // For layers that do not scroll off, meaning the layer has a minHeight, start
                // counting using minHeightBottomOffset. If minHeightBottomOffset already exceeds
                // the total height (e.g. when bottom controls is growing its minHeight with
                // animation), reset it to the total height, so the next layer's bottomOffset
                // will start counting from the bottom of the bottom controls, and layer's yOffset
                // does not exceeds the layer's height.
                minHeightBottomOffset += layer.getHeight();
                layerYOffset = minHeightBottomOffset - mTotalHeight;

                minHeightBottomOffset = Math.min(minHeightBottomOffset, mTotalHeight);
            }

            yOffsetOfLayers.put(type, layerYOffset);
        }

        logIfHeightMismatch(
                "Heights before #repositionLayers",
                mTotalHeight,
                mTotalMinHeight,
                "First pass in #repositionLayers",
                height,
                totalMinHeight);

        // 2. If animated, compare and fix the yOffset with the previous mLayerOffsets if reposition
        // is caused by an animated browser controls height adjustment. This needs to run in a
        // different loop to cooperate browser controls height reduction, as we need to still push
        // updates to layer that's changed from visible -> hidden.
        if (animated) {
            // When bottomOffset is negative, the browser controls is going through a height
            // reduction.
            boolean isControlsShrinking = bottomOffset < 0;

            // Create a initial value for layer's yOffset, in case the top layer is hiding,
            // shrinking the bottom control's height.
            int layerYOffset = bottomOffset - mTotalHeight;
            for (int type : STACK_ORDER) {
                BottomControlsLayer layer = mLayers.get(type);
                if (layer == null) continue;
                // If the layer is not visible, don't account for it for animation calculations
                // unless it's still actively animating to hide.
                if (!mLayerVisibilities.get(type)
                        && layer.getLayerVisibility() != LayerVisibility.HIDING) {
                    continue;
                }

                // Read the yOffset calculated in step #1. If the layer is hiding, use a default
                // value.
                layerYOffset = yOffsetOfLayers.get(type, layerYOffset + layer.getHeight());

                // When the height adjustment is animated, we need to read the previous position
                // offsets decide which layers can be moved.
                int previousYOffset = mLayerYOffsets.get(type, layerYOffset);
                if (isControlsShrinking) {
                    // When browser controls shrinking, none of the layers should move upwards (i.e.
                    // yOffset decrease)
                    layerYOffset = Math.max(layerYOffset, previousYOffset);
                } else {
                    // When browser controls growing, none of the layers should move downward (i.e.
                    // yOffset increase)
                    layerYOffset = Math.min(layerYOffset, previousYOffset);
                }

                yOffsetOfLayers.put(type, layerYOffset);
            }
        }

        // 3. Dispatch the yOffset to each layers. Do this after the calculation is done, so all
        // layers do not change their state during the algorithm.
        for (int layerType : STACK_ORDER) {
            BottomControlsLayer layer = mLayers.get(layerType);
            if (layer == null) continue;

            // Record the current yOffset in case the offset will be used for future animated
            // height adjustment.
            int yOffset = yOffsetOfLayers.get(layerType, layer.getHeight());
            if (!mLayerVisibilities.get(layerType)
                    && layer.getLayerVisibility() != LayerVisibility.HIDING) {
                mLayerYOffsets.delete(layerType);
                yOffset = layer.getHeight();
            } else {
                mLayerYOffsets.put(layerType, yOffset);
            }

            layer.onBrowserControlsOffsetUpdate(yOffset);
            if (sDumpLayerUpdateForTesting) {
                dumpStatsForLayerForTesting(layer, yOffset);
            }
        }
    }

    /** Recalculate the browser controls height based on layer sizes. */
    private void recalculateLayerSizes() {
        int height = 0;
        int minHeight = 0;
        for (int type : STACK_ORDER) {
            BottomControlsLayer layer = mLayers.get(type);
            if (layer == null || !mLayerVisibilities.get(type)) continue;

            boolean shouldScrollOff = shouldLayerScrollOff(layer, minHeight);
            assert minHeight == 0 || !shouldScrollOff
                    : "A scroll-off layer under a NEVER_SCROLL_OFF layer is not supported. Layer: "
                            + layer.getType();

            height += layer.getHeight();
            minHeight += shouldScrollOff ? 0 : layer.getHeight();
        }

        mTotalHeight = height;
        mTotalMinHeight = minHeight;
    }

    /**
     * The layer should scroll off if it is labeled as ALWAYS_SCROLL_OFF, or if it is labeled as
     * DEFAULT_SCROLL_OFF and isn't positioned under a NEVER_SCROLL_OFF layer.
     */
    private static boolean shouldLayerScrollOff(BottomControlsLayer layer, int totalMinHeight) {
        int scrollOffBehavior = layer.getScrollBehavior();
        return (scrollOffBehavior == LayerScrollBehavior.ALWAYS_SCROLL_OFF)
                || (totalMinHeight == 0
                        && scrollOffBehavior == LayerScrollBehavior.DEFAULT_SCROLL_OFF);
    }

    /** Returns whether bottom controls stacker is calculating height. */
    public static boolean isEnabled() {
        return ChromeFeatureList.sBottomBrowserControlsRefactor.isEnabled();
    }

    /** Whether Bottom Controls Stacker is dispatching yOffset. */
    public static boolean isDispatchingYOffset() {
        // This method is used as a kill switch to fallback to the previous behavior.
        return isEnabled()
                && !ChromeFeatureList.sDisableBottomControlsStackerYOffsetDispatching.getValue();
    }

    /**
     * Updates the visibilities of the layers. This is done altogether, since the visibility of some
     * layers may depend on the visibility of others.
     */
    private void updateLayerVisibilities() {
        mLayerVisibilities.clear();
        boolean atLeastOneVisibleLayer = false;
        for (int type : STACK_ORDER) {
            BottomControlsLayer layer = mLayers.get(type);
            if (layer == null) continue;

            if (layer.getLayerVisibility() == LayerVisibility.VISIBLE
                    || layer.getLayerVisibility() == LayerVisibility.SHOWING) {
                atLeastOneVisibleLayer = true;
                break;
            }
        }
        for (int type : STACK_ORDER) {
            BottomControlsLayer layer = mLayers.get(type);
            if (layer == null) continue;

            @LayerVisibility int layerVisibility = layer.getLayerVisibility();
            mLayerVisibilities.put(
                    type,
                    layerVisibility == LayerVisibility.VISIBLE
                            || layer.getLayerVisibility() == LayerVisibility.SHOWING
                            || (atLeastOneVisibleLayer
                                    && layerVisibility
                                            == LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE));
        }
    }

    private static void logIfHeightMismatch(
            String expected,
            int expectedHeight,
            int expectedMinHeight,
            String actual,
            int actualHeight,
            int actualMinHeight) {

        if (expectedHeight == actualHeight && expectedMinHeight == actualMinHeight) return;

        Log.w(
                TAG,
                "Height mismatch observed."
                        + " ["
                        + expected
                        + "]"
                        + " expectedHeight= "
                        + expectedHeight
                        + " expectedMinHeight= "
                        + expectedMinHeight
                        + " ["
                        + actual
                        + "]"
                        + " actualHeight = "
                        + actualHeight
                        + " actualMinHeight= "
                        + actualMinHeight);
    }

    private static void dumpStatsForLayerForTesting(BottomControlsLayer layer, int layerYOffset) {
        Log.d(
                TAG,
                "Layer: "
                        + layer.getType()
                        + " Height "
                        + layer.getHeight()
                        + " YOffset "
                        + layerYOffset);
    }

    public BottomControlsLayer getLayerForTesting(@LayerType int layerType) {
        return mLayers.get(layerType);
    }
}
