// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import android.content.Context;
import android.view.LayoutInflater;

import org.chromium.chrome.R;

/** Helpers for tests outside of this package that need a real {@link CustomTabToolbar}. */
public class CustomTabToolbarTestUtils {
    private CustomTabToolbarTestUtils() {}

    /**
     * Inflates a {@link CustomTabToolbar}, along with its close button, which is otherwise inflated
     * lazily by {@link CustomTabToolbarButtonsViewBinder}.
     */
    public static CustomTabToolbar inflateToolbar(Context context) {
        CustomTabToolbar toolbar =
                (CustomTabToolbar)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.new_custom_tab_toolbar,
                                        /* root= */ null,
                                        /* attachToRoot= */ false);
        toolbar.ensureCloseButtonInflated();
        return toolbar;
    }
}
