// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** List of properties to designate information about module in Safety Hub. */
public class SafetyHubModuleProperties {
    /**
     * Order reflects state severity. Lowest being the most severe state and highest being the
     * safest state. Must be kept in sync with SafetyHubModuleState in settings/enums.xml.
     */
    @IntDef({ModuleState.WARNING, ModuleState.UNAVAILABLE, ModuleState.INFO, ModuleState.SAFE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ModuleState {
        int WARNING = 0;
        int UNAVAILABLE = 1;
        int INFO = 2;
        int SAFE = 3;
        int MAX_VALUE = SAFE;
    }

    /**
     * Values used in "for" loop below - should start from 0 and can't have gaps, lowest value is
     * additionally used for starting loop. Order reflects the way modules should be ordered if they
     * have the same state.
     */
    @IntDef({
        ModuleOption.UPDATE_CHECK,
        ModuleOption.ACCOUNT_PASSWORDS,
        ModuleOption.SAFE_BROWSING,
        ModuleOption.UNUSED_PERMISSIONS,
        ModuleOption.NOTIFICATION_REVIEW,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ModuleOption {
        int UPDATE_CHECK = 0;
        int ACCOUNT_PASSWORDS = 1;
        int SAFE_BROWSING = 2;
        int UNUSED_PERMISSIONS = 3;
        int NOTIFICATION_REVIEW = 4;
        int OPTION_FIRST = UPDATE_CHECK;
        int NUM_ENTRIES = 5;
    }

    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_EXPANDED =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_CONTROLLED_BY_POLICY =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_SIGNED_IN =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<String> ACCOUNT_EMAIL =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            PRIMARY_BUTTON_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            SECONDARY_BUTTON_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            SAFE_STATE_BUTTON_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableIntPropertyKey COMPROMISED_PASSWORDS_COUNT =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey TOTAL_PASSWORDS_COUNT =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<UpdateStatusProvider.UpdateStatus>
            UPDATE_STATUS = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableIntPropertyKey SITES_WITH_UNUSED_PERMISSIONS_COUNT =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.WritableIntPropertyKey SAFE_BROWSING_STATE =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.WritableIntPropertyKey
            NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT = new PropertyModel.WritableIntPropertyKey();

    public static final PropertyKey[] COMMON_SAFETY_HUB_MODULE_KEYS = {
        IS_VISIBLE,
        IS_EXPANDED,
        IS_CONTROLLED_BY_POLICY,
        IS_SIGNED_IN,
        ACCOUNT_EMAIL,
        PRIMARY_BUTTON_LISTENER,
        SECONDARY_BUTTON_LISTENER,
        SAFE_STATE_BUTTON_LISTENER
    };

    public static final PropertyKey[] PASSWORD_CHECK_SAFETY_HUB_MODULE_KEYS =
            PropertyModel.concatKeys(
                    COMMON_SAFETY_HUB_MODULE_KEYS,
                    new PropertyKey[] {COMPROMISED_PASSWORDS_COUNT, TOTAL_PASSWORDS_COUNT});

    public static final PropertyKey[] UPDATE_CHECK_SAFETY_HUB_MODULE_KEYS =
            PropertyModel.concatKeys(
                    COMMON_SAFETY_HUB_MODULE_KEYS, new PropertyKey[] {UPDATE_STATUS});

    public static final PropertyKey[] PERMISSIONS_MODULE_KEYS =
            PropertyModel.concatKeys(
                    COMMON_SAFETY_HUB_MODULE_KEYS,
                    new PropertyKey[] {SITES_WITH_UNUSED_PERMISSIONS_COUNT});

    public static final PropertyKey[] NOTIFICATIONS_REVIEW_MODULE_KEYS =
            PropertyModel.concatKeys(
                    COMMON_SAFETY_HUB_MODULE_KEYS,
                    new PropertyKey[] {NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT});

    public static final PropertyKey[] SAFE_BROWSING_MODULE_KEYS =
            PropertyModel.concatKeys(
                    COMMON_SAFETY_HUB_MODULE_KEYS, new PropertyKey[] {SAFE_BROWSING_STATE});

    public static final PropertyKey[] BROWSER_STATE_MODULE_KEYS = {
        COMPROMISED_PASSWORDS_COUNT,
        TOTAL_PASSWORDS_COUNT,
        UPDATE_STATUS,
        SITES_WITH_UNUSED_PERMISSIONS_COUNT,
        NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
        SAFE_BROWSING_STATE
    };
}
