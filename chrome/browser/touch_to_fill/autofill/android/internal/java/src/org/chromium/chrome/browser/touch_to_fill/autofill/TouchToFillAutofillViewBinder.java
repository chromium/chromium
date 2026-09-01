// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.autofill;

import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.ACKNOWLEDGE_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.SETTINGS_LINK_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.VISIBLE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link TouchToFillAutofillProperties} changes in a {@link
 * PropertyModel} to the suitable method in {@link TouchToFillAutofillView}.
 */
@NullMarked
final class TouchToFillAutofillViewBinder {
    static void bind(PropertyModel model, TouchToFillAutofillView view, PropertyKey propertyKey) {
        if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else if (propertyKey == ACKNOWLEDGE_HANDLER) {
            view.setAcknowledgeHandler(model.get(ACKNOWLEDGE_HANDLER));
        } else if (propertyKey == SETTINGS_LINK_HANDLER) {
            view.setSettingsLinkHandler(model.get(SETTINGS_LINK_HANDLER));
        } else if (propertyKey == DISMISS_HANDLER) {
            view.setDismissHandler(model.get(DISMISS_HANDLER));
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }

    private TouchToFillAutofillViewBinder() {}
}
