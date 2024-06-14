// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import android.util.SparseArray;

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

    /** Enums that defines the type and position for each bottom controls. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({LayerType.BOTTOM_TOOLBAR, LayerType.READ_ALOUD_PLAYER})
    public @interface LayerType {
        int BOTTOM_TOOLBAR = 0;
        int READ_ALOUD_PLAYER = 1;
    }

    /** Enums that defines the scroll behavior for different controls. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({LayerScrollBehavior.SCROLL_OFF, LayerScrollBehavior.NO_SCROLL_OFF})
    public @interface LayerScrollBehavior {
        int SCROLL_OFF = 0;
        int NO_SCROLL_OFF = 1;
    }

    // The pre-defined stack order for different bottom controls.
    private static final @LayerType int[] STACK_ORDER =
            new int[] {LayerType.BOTTOM_TOOLBAR, LayerType.READ_ALOUD_PLAYER};

    private final SparseArray<BottomControlsLayer> mLayers = new SparseArray<>();
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
     * Trigger the browser controls height update based on the current layer status.
     *
     * @param animate Whether animate the browser controls size change.
     */
    public void requestLayerUpdate(boolean animate) {
        assert isEnabled();

        recalculateLayerSizes();
        updateBrowserControlsHeight(animate);
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
        }
    }

    private void recalculateLayerSizes() {
        int height = 0;
        int minHeight = 0;
        for (int type : STACK_ORDER) {
            BottomControlsLayer layer = mLayers.get(type);
            if (layer == null || !layer.isVisible()) continue;

            boolean canScrollOff = layer.getScrollBehavior() == LayerScrollBehavior.SCROLL_OFF;
            assert minHeight == 0 || !canScrollOff
                    : "SCROLL_OFF layer under a NON_SCROLL_OFF layer is not supported. Layer: "
                            + layer.getType();

            height += layer.getHeight();
            minHeight += canScrollOff ? 0 : layer.getHeight();
        }

        mTotalHeight = height;
        mTotalMinHeight = minHeight;
    }

    private static boolean isEnabled() {
        return ChromeFeatureList.sBottomBrowserControlsRefactor.isEnabled();
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
}
