// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;

import org.chromium.components.browser_ui.settings.SettingsUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Interface for the Safety Hub modules' mediators. */
interface SafetyHubModuleMediator {
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

    public void setUpModule();

    public void updateModule();

    public @ModuleState int getModuleState();

    public @ModuleOption int getOption();

    public boolean isManaged();

    public void destroy();

    public void setExpandState(boolean expanded);

    default void setModuleExpandState(boolean noOtherNonManagedWarningState) {
        switch (getModuleState()) {
            case ModuleState.WARNING:
                setExpandState(!isManaged() || !noOtherNonManagedWarningState);
                break;
            case ModuleState.UNAVAILABLE:
            case ModuleState.INFO:
                setExpandState(!noOtherNonManagedWarningState);
                break;
            case ModuleState.SAFE:
                setExpandState(false);
                break;
            default:
                throw new IllegalArgumentException();
        }
    }

    default Drawable getIcon(Context context) {
        switch (getModuleState()) {
            case ModuleState.SAFE:
                return SettingsUtils.getTintedIcon(
                        context, R.drawable.material_ic_check_24dp, R.color.default_green);
            case ModuleState.INFO:
            case ModuleState.UNAVAILABLE:
                return isManaged()
                        ? SafetyHubUtils.getManagedIcon(context)
                        : SettingsUtils.getTintedIcon(
                                context,
                                R.drawable.btn_info,
                                R.color.default_icon_color_secondary_tint_list);
            case ModuleState.WARNING:
                return isManaged()
                        ? SafetyHubUtils.getManagedIcon(context)
                        : SettingsUtils.getTintedIcon(
                                context, R.drawable.ic_error, R.color.default_red);
            default:
                throw new IllegalArgumentException();
        }
    }

    default int getOrder() {
        // Modules are ordered based on the severity of their {@link
        // SafetyHubModuleProperties.ModuleState}. Modules in warning state that are not controlled
        // by policy should appear first in the list. Followed by unavailable, info then safe
        // states.
        // If multiple modules have the same state, fallback to the order in {@link
        // SafetyHubModuleProperties.ModuleOption}.
        @ModuleState int state = getModuleState();
        @ModuleOption int option = getOption();
        switch (state) {
            case ModuleState.SAFE:
            case ModuleState.INFO:
            case ModuleState.UNAVAILABLE:
                return option + (state * ModuleOption.NUM_ENTRIES);
            case ModuleState.WARNING:
                return option
                        + (isManaged()
                                ? (ModuleState.INFO * ModuleOption.NUM_ENTRIES)
                                : (ModuleState.WARNING * ModuleOption.NUM_ENTRIES));
            default:
                throw new IllegalArgumentException();
        }
    }
}
