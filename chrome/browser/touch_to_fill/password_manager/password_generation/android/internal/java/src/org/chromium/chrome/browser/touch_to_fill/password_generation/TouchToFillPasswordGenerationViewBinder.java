// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.password_generation;

import static org.chromium.chrome.browser.touch_to_fill.password_generation.TouchToFillPasswordGenerationProperties.ACCOUNT_EMAIL;
import static org.chromium.chrome.browser.touch_to_fill.password_generation.TouchToFillPasswordGenerationProperties.GENERATED_PASSWORD;
import static org.chromium.chrome.browser.touch_to_fill.password_generation.TouchToFillPasswordGenerationProperties.PASSWORD_ACCEPTED_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.password_generation.TouchToFillPasswordGenerationProperties.PASSWORD_REJECTED_CALLBACK;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Maps the {@link TouchToFillPasswordGenerationProperties} model properties to the {@link
 * TouchToFillPasswordGenerationView}.
 */
class TouchToFillPasswordGenerationViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link TouchToFillPasswordGenerationView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindView(
            PropertyModel model, TouchToFillPasswordGenerationView view, PropertyKey propertyKey) {
        if (propertyKey == ACCOUNT_EMAIL) {
            view.setSheetSubtitle(model.get(ACCOUNT_EMAIL));
        } else if (propertyKey == GENERATED_PASSWORD) {
            view.setSheetTitle(model.get(GENERATED_PASSWORD));
            view.setGeneratedPassword(model.get(GENERATED_PASSWORD));
        } else if (propertyKey == PASSWORD_ACCEPTED_CALLBACK) {
            view.setPasswordAcceptedCallback(model.get(PASSWORD_ACCEPTED_CALLBACK));
        } else if (propertyKey == PASSWORD_REJECTED_CALLBACK) {
            view.setPasswordRejectedCallback(model.get(PASSWORD_REJECTED_CALLBACK));
        }
    }
}
