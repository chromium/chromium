// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ROW_CLICK_RUNNABLE;

import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Forwards changed property values to the view. */
@NullMarked
public class TabGroupListBottomSheetNewGroupRowViewBinder {
    /** Propagates a key from the model to the view. */
    public static void bind(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        if (propertyKey == ROW_CLICK_RUNNABLE) {
            @Nullable Runnable runnable = model.get(ROW_CLICK_RUNNABLE);
            view.setOnClickListener(runnable == null ? null : ignored -> runnable.run());
        }
    }
}
