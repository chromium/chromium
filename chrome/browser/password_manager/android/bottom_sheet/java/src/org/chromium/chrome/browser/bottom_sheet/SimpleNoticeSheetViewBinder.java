// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottom_sheet;

import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.BUTTON_ACTION;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.BUTTON_TITLE;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TEXT;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TITLE;

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
        if (propertyKey == SHEET_TITLE) {
            view.setTitle(model.get(SHEET_TITLE));
        } else if (propertyKey == SHEET_TEXT) {
            view.setText(model.get(SHEET_TEXT));
        } else if (propertyKey == BUTTON_TITLE) {
            view.setButtonText(model.get(BUTTON_TITLE));
        } else if (propertyKey == BUTTON_ACTION) {
            view.setButtonAction(model.get(BUTTON_ACTION));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private SimpleNoticeSheetViewBinder() {}
}
