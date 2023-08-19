// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ACCOUNT_DISPLAY_NAME;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ON_CLICK_HANDLER;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ON_SHOW_EVENT_LISTENER;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.SHOULD_OFFER_SYNC;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.VISIBLE;

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
        if (propertyKey == ON_SHOW_EVENT_LISTENER) {
            view.setOnShowEventListener(model.get(ON_SHOW_EVENT_LISTENER));
        } else if (propertyKey == DISMISS_HANDLER) {
            view.setDismissHandler(model.get(DISMISS_HANDLER));
        } else if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else if (propertyKey == CURRENT_SCREEN) {
            view.setScreen(model.get(CURRENT_SCREEN));
        } else if (propertyKey == SHOULD_OFFER_SYNC) {
            view.setShouldOfferSync(model.get(SHOULD_OFFER_SYNC));
        } else if (propertyKey == ON_CLICK_HANDLER) {
            view.setOnClickHandler(model.get(ON_CLICK_HANDLER));
        } else if (propertyKey == ACCOUNT_DISPLAY_NAME) {
            view.setAccountDisplayName((String) model.get(ACCOUNT_DISPLAY_NAME));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private PasswordMigrationWarningViewBinder() {}
}
