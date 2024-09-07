// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.annotation.SuppressLint;
import android.content.Context;
import android.util.AttributeSet;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.recyclerview.widget.RecyclerView;

/** Base View class for a tab. */
class AccessorySheetTabView extends RecyclerView {
    /** Constructor for inflating from XML. */
    public AccessorySheetTabView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /** Sets a11y focus on the first child, if it is present. */
    @SuppressLint("AccessibilityFocus")
    void requestDefaultA11yFocus() {
        // The default a11y focus can be requested immediately after setting the RecyclerView
        // adapter, at this point children may not be ready yet. The a11y action is delayed
        // to wait for the internal RecyclerView async work to be finished.
        post(
                () -> {
                    if (getChildCount() > 0) {
                        getChildAt(0)
                                .performAccessibilityAction(
                                        AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS,
                                        null);
                    }
                });
    }
}
