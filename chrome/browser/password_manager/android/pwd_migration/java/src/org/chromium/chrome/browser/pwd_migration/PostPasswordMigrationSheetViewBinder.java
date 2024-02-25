// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.chromium.chrome.browser.pwd_migration.PostPasswordMigrationSheetProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.pwd_migration.PostPasswordMigrationSheetProperties.VISIBLE;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link PostPasswordMigrationSheetProperties} changes in a {@link
 * PropertyModel} to the suitable method in {@link PostPasswordMigrationSheetView}.
 */
class PostPasswordMigrationSheetViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link PasswordMigrationWarningView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindPostPasswordMigrationSheetView(
            PropertyModel model, PostPasswordMigrationSheetView view, PropertyKey propertyKey) {
        if (propertyKey == DISMISS_HANDLER) {
            view.setDismissHandler(model.get(DISMISS_HANDLER));
        } else if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private PostPasswordMigrationSheetViewBinder() {}
}
