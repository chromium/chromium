// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;

/** Utility class for Glic UI components. */
@NullMarked
public final class GlicUiUtils {
    private GlicUiUtils() {}

    /**
     * Configures the Glic inactive placeholder view.
     *
     * @param placeholder The placeholder view.
     */
    public static void setupPlaceholderView(TextViewWithCompoundDrawables placeholder) {
        placeholder.setText(R.string.glic_inactive_view_card_text);
        placeholder.setCompoundDrawablesRelativeWithIntrinsicBounds(
                0, R.drawable.ic_spark_filled_24dp, 0, 0);
        placeholder.setDrawableTintColor(
                AppCompatResources.getColorStateList(
                        placeholder.getContext(), R.color.default_icon_color_tint_list));
    }
}
