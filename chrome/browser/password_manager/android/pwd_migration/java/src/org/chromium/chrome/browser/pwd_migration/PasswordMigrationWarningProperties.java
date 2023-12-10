// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Properties defined here reflect the visible state of the local passwords migration warning. */
class PasswordMigrationWarningProperties {
    @IntDef({MigrationOption.SYNC_PASSWORDS, MigrationOption.EXPORT_AND_DELETE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface MigrationOption {
        int SYNC_PASSWORDS = 0;
        int EXPORT_AND_DELETE = 1;
    }

    /** The different screens that can be shown on the sheet. */
    @IntDef({ScreenType.NONE, ScreenType.INTRO_SCREEN, ScreenType.OPTIONS_SCREEN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScreenType {
        int NONE = 0;
        int INTRO_SCREEN = 1;
        int OPTIONS_SCREEN = 2;
    }

    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey("visible");
    public static final ReadableObjectPropertyKey<Runnable> ON_SHOW_EVENT_LISTENER =
            new ReadableObjectPropertyKey<>("on_show_event_listener");
    static final ReadableObjectPropertyKey<Callback<Integer>> DISMISS_HANDLER =
            new ReadableObjectPropertyKey<>("dismiss_handler");
    static final ReadableObjectPropertyKey<PasswordMigrationWarningOnClickHandler>
            ON_CLICK_HANDLER = new ReadableObjectPropertyKey<>("on_click_handler");

    public static final WritableBooleanPropertyKey SHOULD_OFFER_SYNC =
            new WritableBooleanPropertyKey("should_offer_sync");

    public static final WritableIntPropertyKey CURRENT_SCREEN =
            new WritableIntPropertyKey("current_screen");
    public static final WritableObjectPropertyKey ACCOUNT_DISPLAY_NAME =
            new WritableObjectPropertyKey("account_display_name");

    static PropertyModel createDefaultModel(
            Runnable onShowEventListener,
            Callback<Integer> dismissHandler,
            PasswordMigrationWarningOnClickHandler onClickHandler) {
        return new PropertyModel.Builder(
                        VISIBLE,
                        ON_SHOW_EVENT_LISTENER,
                        DISMISS_HANDLER,
                        SHOULD_OFFER_SYNC,
                        ON_CLICK_HANDLER,
                        CURRENT_SCREEN,
                        ACCOUNT_DISPLAY_NAME)
                .with(ON_SHOW_EVENT_LISTENER, onShowEventListener)
                .with(DISMISS_HANDLER, dismissHandler)
                .with(ON_CLICK_HANDLER, onClickHandler)
                .build();
    }

    private PasswordMigrationWarningProperties() {}
}
