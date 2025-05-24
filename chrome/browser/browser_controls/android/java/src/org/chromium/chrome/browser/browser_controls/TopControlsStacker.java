// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

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

    // All controls are stored in a Map and we should only have one of each control type.
    private final Map<@TopControlType Integer, TopControlLayer> mControls;

    public TopControlsStacker() {
        mControls = new HashMap<>();
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
}
