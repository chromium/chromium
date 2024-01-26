// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * The View for the tab resumption module, consisting of a header followed by suggestion tile(s).
 */
public class TabResumptionModuleView extends LinearLayout {
    public TabResumptionModuleView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    void destroy() {}

    /** Reads suggestion bundle from `model`, render if non-null. */
    public void renderFromModel(PropertyModel model) {
        SuggestionBundle bundle =
                (SuggestionBundle) model.get(TabResumptionModuleProperties.SUGGESTION_BUNDLE);
        if (bundle == null) {
            // TODO(crbug.com/1515325): Remove all tiles.
        } else {
            // TODO(crbug.com/1515325): Fetch images and render all tiles.
        }
    }
}
