// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

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

    /**
     * Constructs the top controls stacker, which is used to calculate heights and offsets for any
     * top controls.
     *
     * @param browserControlsSizer {@link BrowserControlsSizer} to request browser controls changes.
     */
    public TopControlsStacker(BrowserControlsSizer browserControlsSizer) {
        mControls = new HashMap<>();
        mBrowserControlsSizer = browserControlsSizer;
        mBrowserControlsSizer.addObserver(this);
    }

    /**
     * Adds a new control layer to the list of active top controls.
     *
     * @param newControl TopControlLayer to add to the active controls.
     */
    public void addControl(TopControlLayer newControl) {
        assert mControls.get(newControl.getTopControlType()) == null
                : "Trying to add a duplicate control type.";
        mControls.put(newControl.getTopControlType(), newControl);
    }

    /**
     * Removes a control layer from the list of active top controls.
     *
     * @param control The TopControlLayer to remove from the active controls.
     */
    public void removeControl(TopControlLayer control) {
        mControls.remove(control.getTopControlType());
    }

    /**
     * Returns the total height of all currently visible {@link TopControlLayer} controls of this
     * instance that also contribute to the total height of the controls.
     *
     * @return The total height of all visible controls in pixels.
     */
    public int getVisibleTopControlsTotalHeight() {
        int totalHeight = 0;
        for (@TopControlType int type : STACK_ORDER) {
            TopControlLayer layer = mControls.get(type);
            if (layer == null || !layer.contributesToTotalHeight()) continue;
            if (layer.getTopControlVisibility() == TopControlVisibility.VISIBLE) {
                totalHeight += layer.getTopControlHeight();
            }
        }
        return totalHeight;
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

    /** Tear down |this| and clear all existing controls from the Map. */
    public void destroy() {
        mControls.clear();
        mBrowserControlsSizer.removeObserver(this);
    }
}
