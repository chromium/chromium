// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableLongPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

class SafetyCheckProperties {
    /** State of the Safe Browsing check, one of the {@link SafeBrowsingState} values. */
    static final WritableIntPropertyKey SAFE_BROWSING_STATE = new WritableIntPropertyKey();

    /** State of the updates check, one of the {@link UpdatesState} values. */
    static final WritableIntPropertyKey UPDATES_STATE = new WritableIntPropertyKey();

    /** Listener for the Safe Browsing element click events. */
    static final WritableObjectPropertyKey SAFE_BROWSING_CLICK_LISTENER =
            new WritableObjectPropertyKey();

    /** Listener for the updates element click events. */
    static final WritableObjectPropertyKey UPDATES_CLICK_LISTENER = new WritableObjectPropertyKey();

    /** Listener for Safety check button click events. */
    static final WritableObjectPropertyKey SAFETY_CHECK_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey();

    /** Timestamp of the last run, a Long object. */
    static final WritableLongPropertyKey LAST_RUN_TIMESTAMP = new WritableLongPropertyKey();

    @IntDef({
        SafeBrowsingState.UNCHECKED,
        SafeBrowsingState.CHECKING,
        SafeBrowsingState.ENABLED_STANDARD,
        SafeBrowsingState.ENABLED_ENHANCED,
        SafeBrowsingState.DISABLED,
        SafeBrowsingState.DISABLED_BY_ADMIN,
        SafeBrowsingState.ERROR
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface SafeBrowsingState {
        int UNCHECKED = 0;
        int CHECKING = 1;
        int ENABLED_STANDARD = 2;
        int ENABLED_ENHANCED = 3;
        int DISABLED = 4;
        int DISABLED_BY_ADMIN = 5;
        int ERROR = 6;
    }

    static @SafeBrowsingState int safeBrowsingStateFromNative(@SafeBrowsingStatus int status) {
        switch (status) {
            case SafeBrowsingStatus.CHECKING:
                return SafeBrowsingState.CHECKING;
            case SafeBrowsingStatus.ENABLED:
            case SafeBrowsingStatus.ENABLED_STANDARD:
            case SafeBrowsingStatus.ENABLED_STANDARD_AVAILABLE_ENHANCED:
                return SafeBrowsingState.ENABLED_STANDARD;
            case SafeBrowsingStatus.ENABLED_ENHANCED:
                return SafeBrowsingState.ENABLED_ENHANCED;
            case SafeBrowsingStatus.DISABLED:
                return SafeBrowsingState.DISABLED;
            case SafeBrowsingStatus.DISABLED_BY_ADMIN:
                return SafeBrowsingState.DISABLED_BY_ADMIN;
            case SafeBrowsingStatus.DISABLED_BY_EXTENSION:
                assert false : "Safe Browsing cannot be disabled by extension on Android.";
                return SafeBrowsingState.UNCHECKED;
            default:
                assert false : "Unknown SafeBrowsingStatus value.";
        }
        // Never reached.
        return SafeBrowsingState.UNCHECKED;
    }

    @IntDef({
        UpdatesState.UNCHECKED,
        UpdatesState.CHECKING,
        UpdatesState.UPDATED,
        UpdatesState.OUTDATED,
        UpdatesState.OFFLINE,
        UpdatesState.ERROR
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface UpdatesState {
        int UNCHECKED = 0;
        int CHECKING = 1;
        int UPDATED = 2;
        int OUTDATED = 3;
        int OFFLINE = 4;
        int ERROR = 5;
    }

    static @UpdateStatus int updatesStateToNative(@UpdatesState int state) {
        switch (state) {
            case UpdatesState.UNCHECKED:
                // This is not used.
                assert false : "UpdatesState.UNCHECKED has no native equivalent.";
                return UpdateStatus.FAILED;
            case UpdatesState.CHECKING:
                return UpdateStatus.CHECKING;
            case UpdatesState.UPDATED:
                return UpdateStatus.UPDATED;
            case UpdatesState.OUTDATED:
                return UpdateStatus.OUTDATED;
            case UpdatesState.OFFLINE:
                return UpdateStatus.FAILED_OFFLINE;
            case UpdatesState.ERROR:
                return UpdateStatus.FAILED;
            default:
                assert false : "Unknown UpdatesState value.";
                return UpdateStatus.FAILED;
        }
    }

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                SAFE_BROWSING_STATE,
                UPDATES_STATE,
                SAFE_BROWSING_CLICK_LISTENER,
                UPDATES_CLICK_LISTENER,
                SAFETY_CHECK_BUTTON_CLICK_LISTENER,
                LAST_RUN_TIMESTAMP
            };

    static PropertyModel createSafetyCheckModel() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(SAFE_BROWSING_STATE, SafeBrowsingState.UNCHECKED)
                .with(UPDATES_STATE, UpdatesState.UNCHECKED)
                .with(LAST_RUN_TIMESTAMP, 0)
                .build();
    }
}
