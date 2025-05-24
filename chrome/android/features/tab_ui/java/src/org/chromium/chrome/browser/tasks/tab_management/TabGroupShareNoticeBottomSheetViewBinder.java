// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupShareNoticeBottomSheetProperties.COMPLETION_HANDLER;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Forwards changed property values to the view. */
@NullMarked
public class TabGroupShareNoticeBottomSheetViewBinder {
    /** Propagates a key from the model to the view. */
    public static void bind(
            PropertyModel model, TabGroupShareNoticeBottomSheetView view, PropertyKey propertyKey) {
        if (propertyKey == COMPLETION_HANDLER) {
            view.setCompletionHandler(model.get(COMPLETION_HANDLER));
        }
    }
}
