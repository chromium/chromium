// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import android.content.Context;
import android.util.SparseArray;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.OffsetTagConstraints;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayUtil;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Coordinator class for UI layers in the bottom browser controls. This class manages the relative
 * y-axis position for every registered bottom control elements, and their background colors.
 *
 * <p>Background colors are automatically coordinated based on layer positioning - the bottom-most
 * visible layer that provides a background color will be used for the entire bottom controls.
 */
@NullMarked
public class BottomControlsStacker implements BrowserControlsStateProvider.Observer {
    private static final String TAG = "BotControlsStacker";

    public static final int INVALID_HEIGHT = -1;

    private static boolean sDumpLayerUpdateForTesting;
    private int mNumberOfVisibleLayers;

    /** Enums that defines the type and position for each bottom controls. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        LayerType.PROGRESS_BAR,
        LayerType.TABSTRIP_TOOLBAR,
        LayerType.TABSTRIP_TOOLBAR_BELOW_READALOUD,
        LayerType.READ_ALOUD_PLAYER,
        LayerType.BOTTOM_TOOLBAR,
        LayerType.BOTTOM_CHIN,
        LayerType.TEST_BOTTOM_LAYER
    })
    public @interface LayerType {
        // The progress bar during page loading. This layer has a height of 0 and overlaps the next
        // visible layer in the stack.
        int PROGRESS_BAR = 0;
        int TABSTRIP_TOOLBAR = 1;
        int READ_ALOUD_PLAYER = 2;
        // Temporary layer that allows us to flag guard the new behavior of stacking the tabstrip
        // toolbar below, rather than above, the readadloud player.
        int TABSTRIP_TOOLBAR_BELOW_READALOUD = 3;
        int BOTTOM_TOOLBAR = 4;
        int BOTTOM_CHIN = 5;

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
                LayerType.PROGRESS_BAR,
                LayerType.TABSTRIP_TOOLBAR,
                LayerType.READ_ALOUD_PLAYER,
                LayerType.TABSTRIP_TOOLBAR_BELOW_READALOUD,
                LayerType.BOTTOM_TOOLBAR,
                LayerType.BOTTOM_CHIN,
                LayerType.TEST_BOTTOM_LAYER
            };

    private final SparseArray<BottomControlsLayer> mLayers = new SparseArray<>();
    // Recorded the yOffset for all current layers. This only record the yOffset for visible layers.
    private final SparseIntArray mLayerYOffsets = new SparseIntArray();
    private final SparseBooleanArray mLayerVisibilities = new SparseBooleanArray();

    // The heights of each layer at their fully shown positions.
    private final SparseIntArray mLayerRestingOffsets = new SparseIntArray();

    // Whether layer is contributing to the minHeight. This is calculated during height calculation,
    // and won't update when the layers are being repositioned during scroll.
    private final SparseBooleanArray mLayerHasMinHeight = new SparseBooleanArray();
    private boolean mHasMoreThanOneNonScrollableLayer;

    private final BrowserControlsSizer mBrowserControlsSizer;

    private int mTotalHeight = INVALID_HEIGHT;
    private int mTotalMinHeight = INVALID_HEIGHT;

    private @Nullable BrowserControlsOffsetTagsInfo mOffsetTagsInfo;

    private @ColorInt int mCurrentBackgroundColor;

    // The default state is used before any visibility constraint changes occur (ex. reopening
    // chrome after it has been closed.) It must be set to SHOWN to allow the browser to initialize
    // the UI models with the correct y offsets.
    private @BrowserControlsState int mBrowserControlsState = BrowserControlsState.SHOWN;
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;

    /**
     * Construct the coordination class that's used to position different UIs into the bottom
     * controls.
     *
     * @param browserControlsSizer {@link BrowserControlsSizer} to request browser controls changes.
     * @param context Context in which the stacker is operating.
     * @param windowAndroid The window in which the bottom controls stack is displaying.
     */
    public BottomControlsStacker(
            BrowserControlsSizer browserControlsSizer,
            Context context,
            WindowAndroid windowAndroid) {
        mBrowserControlsSizer = browserControlsSizer;
        mBrowserControlsSizer.addObserver(this);
        mContext = context;
        mWindowAndroid = windowAndroid;
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

    /** Returns whether the layer of the given type is visible. */
    public boolean isLayerVisible(@LayerType int layerType) {
        return mLayers.get(layerType) != null && mLayerVisibilities.get(layerType);
    }

    /** Returns the calculated total height of all visible layers. */
    public int getTotalHeight() {
        return mTotalHeight;
    }

    /** Returns the calculated total min height of all visible layers. */
    public int getTotalMinHeight() {
        return mTotalMinHeight;
    }

    /**
     * Whether the layer with {@link type} is not scrollable. To other words, return true iff the
     * layer is contributing to the bottom control's minHeight.
     */
    public boolean isLayerNonScrollable(int type) {
        return mLayers.get(type) != null && mLayerHasMinHeight.get(type);
    }

    /**
     * Whether there are more than one layer that returns true with {@link #isLayerNonScrollable}.
     * To other words, returns true when more than one layer is contributing to browser control's
     * minHeight.
     */
    public boolean hasMultipleNonScrollableLayer() {
        return mHasMoreThanOneNonScrollableLayer;
    }

    private boolean isVisibilityForced() {
        return mBrowserControlsState == BrowserControlsState.HIDDEN
                || mBrowserControlsState == BrowserControlsState.SHOWN;
    }

    /**
     * Trigger the browser controls height update based on the current layer status. If there's
     * already an animated transition running, this call might cause it to skip to the end state.
     *
     * @param animate Whether animate the browser controls size change.
     */
    public void requestLayerUpdate(boolean animate) {
        updateLayerVisibilitiesAndSizes();
        updateBrowserControlsHeight(animate);
        updateBackgroundColorFromLayers();
        if (mBrowserControlsSizer.offsetOverridden()) {
            repositionLayers(
                    mBrowserControlsSizer.getBottomControlOffset(),
                    mBrowserControlsSizer.getBottomControlsMinHeightOffset(),
                    animate,
                    isVisibilityForced());
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
     * @see BrowserControlsSizer#notifyBackgroundColor(int).
     */
    public void notifyBackgroundColor(@ColorInt int color) {
        mCurrentBackgroundColor = color;
        mBrowserControlsSizer.notifyBackgroundColor(color);
    }

    /**
     * Updates the background color based on the currently visible layers. The color is determined
     * by the bottom-most visible layer that provides a background color.
     */
    private void updateBackgroundColorFromLayers() {
        @ColorInt int newBackgroundColor = 0;

        // Find the bottom-most visible layer that provides a background color
        // Iterate through layers in reverse stack order (bottom to top).
        for (int i = STACK_ORDER.length - 1; i >= 0; i--) {
            int layerType = STACK_ORDER[i];
            BottomControlsLayer layer = mLayers.get(layerType);

            if (layer == null || !mLayerVisibilities.get(layerType)) {
                continue;
            }

            Integer layerColor = layer.getBackgroundColor();
            if (layerColor != null && layerColor != 0) {
                newBackgroundColor = layerColor;
                break;
            }
        }

        // Only notify if the color has changed.
        // TODO(crbug.com/430084697): Properly handle cases when newBackgroundColor == 0.
        if (newBackgroundColor != mCurrentBackgroundColor && newBackgroundColor != 0) {
            mCurrentBackgroundColor = newBackgroundColor;
            mBrowserControlsSizer.notifyBackgroundColor(mCurrentBackgroundColor);
        }
    }

    /**
     * Notifies that the active tab has completed a cross-document navigation in the main fraime.
     */
    public void notifyDidFinishNavigationInPrimaryMainFrame() {
        recordLayerMetrics();
    }

    /** Destroy this instance and release the dependencies over the browser controls. */
    public void destroy() {
        mLayers.clear();
        mBrowserControlsSizer.removeObserver(this);
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        // If animations are enabled, calls to #onControlsOffsetChanged will reposition the
        // layers. If animations aren't enabled, no such calls will occur, and #repositionLayers
        // should be triggered here.
        if (!mBrowserControlsSizer.shouldAnimateBrowserControlsHeightChanges()) {
            repositionLayers(
                    mBrowserControlsSizer.getBottomControlOffset(),
                    mBrowserControlsSizer.getBottomControlsMinHeightOffset(),
                    false,
                    isVisibilityForced());
        }
    }

    @Override
    public void onOffsetTagsInfoChanged(
            BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
            BrowserControlsOffsetTagsInfo offsetTagsInfo,
            @BrowserControlsState int constraints,
            boolean shouldUpdateOffsets) {
        mBrowserControlsState = constraints;
        if (ChromeFeatureList.sBcivBottomControls.isEnabled()) {
            mOffsetTagsInfo = offsetTagsInfo;
            int additionalHeight = 0;
            for (int layerType : STACK_ORDER) {
                BottomControlsLayer layer = mLayers.get(layerType);
                if (layer == null) continue;

                if (isLayerNonScrollable(layer.getType())) {
                    layer.clearOffsetTag();
                } else {
                    additionalHeight += layer.updateOffsetTag(offsetTagsInfo);
                }
            }

            int totalHeight = mTotalHeight;
            if (totalHeight == INVALID_HEIGHT) {
                // TODO(crbug.com/463962392): Investigate if this causes any other bugs.
                Log.w(TAG, "Using mTotalHeight before initialization");

                totalHeight = 0;
            }

            mBrowserControlsSizer.setBottomControlsAdditionalHeight(additionalHeight);
            offsetTagsInfo.mBottomControlsConstraints =
                    new OffsetTagConstraints(0, 0, 0, totalHeight + additionalHeight);

            if (shouldUpdateOffsets) {
                repositionLayers(
                        mBrowserControlsSizer.getBottomControlOffset(),
                        mBrowserControlsSizer.getBottomControlsMinHeightOffset(),
                        false,
                        isVisibilityForced());
            }
        }
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
        if (mLayers.size() == 0) return;
        repositionLayers(
                bottomOffset,
                bottomControlsMinHeightOffset,
                requestNewFrame,
                isVisibilityForced || requestNewFrame);
    }

    /** Reposition the layers given that the height and minHeight is known. */
    private void repositionLayers(
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean animated,
            boolean offsetsAppliedByBrowser) {

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

            int layerYOffset;

            // The position of a layer is determined by the sum of its height and renderer's offset.
            // Height refers to the distance from the bottom of the layer when it's fully shown, to
            // the bottom of the screen. An offset can be specified by the renderer to deviate the
            // layer from its height for a scroll or animation. When this happens, the layer is
            // moved over time by using its offset, though its height stays constant.
            //
            // BCIV should take over when offsets are not applied by the browser. This means the
            // controls are free to be scrolled and the offsets in the renderer will be applied by
            // viz. For the offsets to be correctly applied, the browser frame needs to have the
            // controls in their fully visible positions, so we always set the layer's yOffset to be
            // its height,
            //
            // When the offsets are applied by the browser, the browser should be in full control of
            // the layers' positions, and the behavior is identical to having BCIV disabled.
            if (ChromeFeatureList.sBcivBottomControls.isEnabled() && !offsetsAppliedByBrowser) {
                layerYOffset = mLayerRestingOffsets.get(type);
            } else {
                boolean shouldScrollOff = shouldLayerScrollOff(layer, totalMinHeight);
                assert totalMinHeight == 0 || !shouldScrollOff
                        : "A scroll-off layer under a NEVER_SCROLL_OFF layer is not supported."
                                + " Layer: "
                                + layer.getType();

                // 1. Accumulate the layer's height to ensure the height does not change during
                // layout update. This is only used for assertion.
                height += layer.getHeight();
                totalMinHeight += shouldScrollOff ? 0 : layer.getHeight();

                if (shouldScrollOff) {
                    // [Scrollable layers]
                    // Increase the layerBottomOffset so it represents the bottomOffset from the
                    // bottom edge of the layer. The bottom edge of this layer can sit lower in the
                    // controls than the next layer's top edge if the next layer does not scroll
                    // off, so set the minValue from the minHeightBottomOffset.
                    layerBottomOffset += layer.getHeight();
                    layerYOffset = layerBottomOffset - mTotalHeight;

                    layerBottomOffset = Math.min(layerBottomOffset, minHeightBottomOffset);
                } else {
                    // [Non scrollable layers]
                    // For layers that do not scroll off, meaning the layer has a minHeight, start
                    // counting using minHeightBottomOffset. If minHeightBottomOffset already
                    // exceeds the total height (e.g. when bottom controls is growing its minHeight
                    // with animation), reset it to the total height, so the next layer's
                    // bottomOffset will start counting from the bottom of the bottom controls, and
                    // layer's yOffset does not exceeds the layer's height.
                    minHeightBottomOffset += layer.getHeight();
                    layerYOffset = minHeightBottomOffset - mTotalHeight;

                    minHeightBottomOffset = Math.min(minHeightBottomOffset, mTotalHeight);
                }


                logIfHeightMismatch(
                        "Heights before #repositionLayers",
                        mTotalHeight,
                        mTotalMinHeight,
                        "First pass in #repositionLayers",
                        height,
                        totalMinHeight);
            }

            yOffsetOfLayers.put(type, layerYOffset);
        }


        // 2. If animated, compare and fix the yOffset with the previous mLayerOffsets if reposition
        // is caused by an animated browser controls height adjustment. This needs to run in a
        // different loop to cooperate browser controls height reduction, as we need to still push
        // updates to layer that's changed from visible -> hidden.
        //
        // TODO(clhager) This block was implemented with the assumption that `animated` would be
        // true if heights/offsets were changing due to an animation. This assumption is incorrect,
        // and `animated` is false when animations happen while browser controls are scrolled off
        // the screen. `animated` is only true when the android views for the browser controls are
        // visible, or when there is a browser driven animation in progress (meaning there are no
        // composited views present.)
        if (animated && bottomOffset != 0) {
            // When bottomOffset is negative, the browser controls is going through a height
            // reduction.
            //
            // TODO(clhager) Controls could be shrinking even if bottomOffset is negative.
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

    /**
     * Recalculates layer visibilities and sizes without mutating bottom controls height or actually
     * repositioning layers. A call to this method must be followed by a call to
     * requestLayerUpdate() in the same stack frame to avoid inconsistency between
     * BottomControlsStacker's state and the state of individual layers. This is useful if you need
     * to mutate browser controls height(s) *before* BottomControlsStacker, e.g. animating a
     * simultaneous top and bottom height change.
     */
    public void updateLayerVisibilitiesAndSizes() {
        updateLayerVisibilities();
        recalculateLayerSizes();
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

            // When min height exists before processing the current layer's height, it means more
            // than one non-scrollable layer exists.
            mHasMoreThanOneNonScrollableLayer = minHeight != 0;

            if (ChromeFeatureList.sBcivBottomControls.isEnabled()) {
                if (shouldScrollOff) {
                    if (mOffsetTagsInfo != null) {
                        layer.updateOffsetTag(mOffsetTagsInfo);
                    }
                } else {
                    layer.clearOffsetTag();
                }
            }

            height += layer.getHeight();
            minHeight += shouldScrollOff ? 0 : layer.getHeight();
            mLayerHasMinHeight.put(type, !shouldScrollOff);
        }

        mTotalHeight = height;
        mTotalMinHeight = minHeight;

        recalculateLayerRestingOffsets();
    }

    /**
     * Calculates the total height of the UI from the specified layer to the bottom.
     *
     * <p>This method computes the cumulative height of all visible layers starting from the given
     * layer **(inclusive)**, down the stack to the bottom-most layer
     *
     * <p><b>Warning:</b> The height returned might not be accurate during {@link
     * #recalculateLayerSizes()}, so it should not be used to determine a layer's attribute.
     *
     * @param startLayer the layer in the stack order to start from.
     * @return the total height of the visible UI from the specified layer to the bottom, or {@link
     *     #INVALID_HEIGHT} if the layer type is invalid.
     */
    public int getHeightFromLayerToBottom(@LayerType int startLayer) {
        if (mLayers.get(startLayer) == null) {
            return INVALID_HEIGHT;
        }

        int height = 0;
        for (int i = 0; i < STACK_ORDER.length; i++) {
            int type = STACK_ORDER[i];
            if (type < startLayer) continue;

            BottomControlsLayer layer = mLayers.get(type);
            if (layer == null || !mLayerVisibilities.get(type)) continue;

            height += layer.getHeight();
        }

        return height;
    }

    private void recalculateLayerRestingOffsets() {
        int cumulativeHeight = 0;
        for (int i = STACK_ORDER.length - 1; i >= 0; i--) {
            int type = STACK_ORDER[i];
            BottomControlsLayer layer = mLayers.get(type);
            if (layer == null || !mLayerVisibilities.get(type)) continue;

            // Offset is with respect to the bottom of the screen, so the offset for the current
            // layer is the negative sum of the heights of all layers below it.
            mLayerRestingOffsets.put(type, -cumulativeHeight);
            cumulativeHeight += layer.getHeight();
        }
    }

    private void recordLayerMetrics() {
        RecordHistogram.recordSparseHistogram(
                "Android.BottomControlsStacker.NumberOfVisibleLayers", mNumberOfVisibleLayers);

        int windowHeight =
                DisplayUtil.dpToPx(
                        mWindowAndroid.getDisplay(),
                        mContext.getResources().getConfiguration().screenHeightDp);
        if (windowHeight == 0) return;
        int percentageOfScreenUsedByBottomControlsMaxHeight = (mTotalHeight / windowHeight) * 100;
        int percentageOfScreenUsedByBottomControlsMinHeight =
                (mTotalMinHeight / windowHeight) * 100;
        RecordHistogram.recordPercentageHistogram(
                "Android.BottomControlsStacker.PercentageOfWindowUsedByBottomControlsAtMaxHeight",
                percentageOfScreenUsedByBottomControlsMaxHeight);
        RecordHistogram.recordPercentageHistogram(
                "Android.BottomControlsStacker.PercentageOfWindowUsedByBottomControlsAtMinHeight",
                percentageOfScreenUsedByBottomControlsMinHeight);
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

    /**
     * Updates the visibilities of the layers. This is done altogether, since the visibility of some
     * layers may depend on the visibility of others.
     */
    private void updateLayerVisibilities() {
        mLayerVisibilities.clear();
        mNumberOfVisibleLayers = 0;
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
            boolean isLayerVisible =
                    layerVisibility == LayerVisibility.VISIBLE
                            || layer.getLayerVisibility() == LayerVisibility.SHOWING
                            || (atLeastOneVisibleLayer
                                    && layerVisibility
                                            == LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE);
            mLayerVisibilities.put(type, isLayerVisible);
            if (isLayerVisible) ++mNumberOfVisibleLayers;
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

    public @Nullable BottomControlsLayer getLayerForTesting(@LayerType int layerType) {
        return mLayers.get(layerType);
    }
}
