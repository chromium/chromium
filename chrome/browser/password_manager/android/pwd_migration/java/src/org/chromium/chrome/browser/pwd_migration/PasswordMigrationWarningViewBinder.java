// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.VISIBLE;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link PasswordMigrationWarningProperties} changes in a {@link
 * PropertyModel} to the suitable method in {@link PasswordMigrationWarningView}.
 */
class PasswordMigrationWarningViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link PasswordMigrationWarningView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindPasswordMigrationWarningView(
            PropertyModel model, PasswordMigrationWarningView view, PropertyKey propertyKey) {
        if (propertyKey == DISMISS_HANDLER) {
            view.setDismissHandler(model.get(DISMISS_HANDLER));
        } else if (propertyKey == VISIBLE) {
            boolean visibilityChangeSuccessful = view.setVisible(model.get(VISIBLE));
            if (!visibilityChangeSuccessful && model.get(VISIBLE)) {
                assert model.get(DISMISS_HANDLER) != null;
                model.get(DISMISS_HANDLER).onResult(BottomSheetController.StateChangeReason.NONE);
            }
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private PasswordMigrationWarningViewBinder() {}
}
