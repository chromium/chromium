// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottom_sheet;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link SimpleNoticeSheetProperties} changes in a {@link
 * PropertyModel} to the suitable method in {@link SimpleNoticeSheetView}.
 */
class SimpleNoticeSheetViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link PSimpleNoticeSheetView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindSimpleNoticeSheetView(
            PropertyModel model, SimpleNoticeSheetView view, PropertyKey propertyKey) {
        // TODO: crbug.com/353283268 - Add properties.
        assert false : "Unhandled update to property:" + propertyKey;
    }

    private SimpleNoticeSheetViewBinder() {}
}
