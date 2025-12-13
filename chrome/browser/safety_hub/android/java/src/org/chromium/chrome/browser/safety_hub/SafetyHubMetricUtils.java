// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.annotation.IntDef;
import androidx.annotation.StringDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleOption;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.StringJoiner;

/** Helper utils to log UMA histograms for Safety Hub. */
@NullMarked
public class SafetyHubMetricUtils {
    @VisibleForTesting
    public static final String EXTERNAL_INTERACTIONS_HISTOGRAM_NAME =
            "Settings.SafetyHub.ExternalInteractions";

    @VisibleForTesting
    public static final String PERMISSIONS_INTERACTIONS_HISTOGRAM_NAME =
            "Settings.SafetyCheck.UnusedSitePermissionsModuleInteractions";

    @VisibleForTesting
    public static final String NOTIFICATIONS_INTERACTIONS_HISTOGRAM_NAME =
            "Settings.SafetyHub.NotificationPermissionsModule.Interactions";

    @VisibleForTesting
    public static final String DASHBOARD_INTERACTIONS_HISTOGRAM_NAME =
            "Settings.SafetyHub.Dashboard.Interactions";

    @VisibleForTesting
    public static final String MODULE_STATE_HISTOGRAM_NAME = "Settings.SafetyHub";

    @VisibleForTesting
    public static final String ABUSIVE_NOTIFICATION_REVOCATION_INTERACTIONS_HISTOGRAM_NAME =
            "Settings.SafetyHub.AbusiveNotificationPermissionRevocation.Interactions";

    /**
     * Interactions on surfaces outside of the Safety Hub settings pages. These can be in the Magic
     * Stack or in other settings surfaces. Must be kept in sync with SafetyHubExternalInteractions
     * in settings/enums.xml.
     */
    @IntDef({
        ExternalInteractions.OPEN_FROM_SETTINGS_PAGE,
        ExternalInteractions.OPEN_FROM_MAGIC_STACK,
        ExternalInteractions.OPEN_SAFE_BROWSING_FROM_MAGIC_STACK,
        ExternalInteractions.OPEN_GPM_FROM_MAGIC_STACK,
        ExternalInteractions.MAX_VALUE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ExternalInteractions {
        int OPEN_FROM_SETTINGS_PAGE = 0;
        int OPEN_FROM_MAGIC_STACK = 1;
        int OPEN_SAFE_BROWSING_FROM_MAGIC_STACK = 2;
        int OPEN_GPM_FROM_MAGIC_STACK = 3;
        int MAX_VALUE = OPEN_GPM_FROM_MAGIC_STACK;
    }

    /**
     * All module types that can appear on the Safety Hub dashboard, including the browser state
     * module. Must be kept in sync with SafetyHubDashboardModuleType in settings/histograms.xml.
     */
    @StringDef({
        DashboardModuleType.UPDATE_CHECK,
        DashboardModuleType.ACCOUNT_PASSWORDS,
        DashboardModuleType.LOCAL_PASSWORDS,
        DashboardModuleType.UNIFIED_PASSWORDS,
        DashboardModuleType.SAFE_BROWSING,
        DashboardModuleType.REVOKED_PERMISSIONS,
        DashboardModuleType.NOTIFICATION_REVIEW,
        DashboardModuleType.BROWSER_STATE
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface DashboardModuleType {
        String UPDATE_CHECK = "UpdateCheck";
        String ACCOUNT_PASSWORDS = "AccountPasswords";
        String LOCAL_PASSWORDS = "LocalPasswords";
        String UNIFIED_PASSWORDS = "UnifiedPasswords";
        String SAFE_BROWSING = "SafeBrowsing";
        String REVOKED_PERMISSIONS = "RevokedPermissions";
        String NOTIFICATION_REVIEW = "NotificationReview";
        String BROWSER_STATE = "BrowserState";
    }

    /**
     * The moment when the dashboard state metrics are logged. Currently we log the state of each
     * module only page impression and exit. Must be kept in sync with SafetyHubLifecycleEvent in
     * settings/histograms.xml.
     */
    @StringDef({LifecycleEvent.ON_IMPRESSION, LifecycleEvent.ON_EXIT})
    @Retention(RetentionPolicy.SOURCE)
    @interface LifecycleEvent {
        String ON_IMPRESSION = "OnImpression";
        String ON_EXIT = "OnExit";
    }

    /**
     * All user interactions that can happen in the permissions review subpage or the permissions
     * review dashboard module. Must be kept in sync with
     * SafetyCheckUnusedSitePermissionsModuleInteractions in settings/enums.xml.
     */
    @IntDef({
        PermissionsModuleInteractions.OPEN_REVIEW_UI,
        PermissionsModuleInteractions.ALLOW_AGAIN,
        PermissionsModuleInteractions.ACKNOWLEDGE_ALL,
        PermissionsModuleInteractions.UNDO_ALLOW_AGAIN,
        PermissionsModuleInteractions.UNDO_ACKNOWLEDGE_ALL,
        PermissionsModuleInteractions.MINIMIZE_REVIEW_UI,
        PermissionsModuleInteractions.GO_TO_SETTINGS,
        PermissionsModuleInteractions.MAX_VALUE
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface PermissionsModuleInteractions {
        int OPEN_REVIEW_UI = 0;
        int ALLOW_AGAIN = 1;
        int ACKNOWLEDGE_ALL = 2;
        int UNDO_ALLOW_AGAIN = 3;
        int UNDO_ACKNOWLEDGE_ALL = 4;
        int MINIMIZE_REVIEW_UI = 5;
        int GO_TO_SETTINGS = 6;
        int MAX_VALUE = GO_TO_SETTINGS;
    }

    /**
     * All user interactions that can happen in the notifications review subpage or the
     * notifications review dashboard module. Must be kipe in sync with
     * SafetyCheckNotificationsModuleInteractions in settings/enums.xml.
     */
    @IntDef({
        NotificationsModuleInteractions.BLOCK,
        NotificationsModuleInteractions.BLOCK_ALL,
        NotificationsModuleInteractions.IGNORE,
        NotificationsModuleInteractions.MINIMIZE,
        NotificationsModuleInteractions.RESET,
        NotificationsModuleInteractions.UNDO_BLOCK,
        NotificationsModuleInteractions.UNDO_IGNORE,
        NotificationsModuleInteractions.UNDO_RESET,
        NotificationsModuleInteractions.OPEN_UI_REVIEW,
        NotificationsModuleInteractions.UNDO_BLOCK_ALL,
        NotificationsModuleInteractions.GO_TO_SETTINGS,
        NotificationsModuleInteractions.MAX_VALUE
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface NotificationsModuleInteractions {
        int BLOCK = 0;
        int BLOCK_ALL = 1;
        int IGNORE = 2;
        int MINIMIZE = 3;
        int RESET = 4;
        int UNDO_BLOCK = 5;
        int UNDO_IGNORE = 6;
        int UNDO_RESET = 7;
        int OPEN_UI_REVIEW = 8;
        int UNDO_BLOCK_ALL = 9;
        int GO_TO_SETTINGS = 10;
        int MAX_VALUE = GO_TO_SETTINGS;
    }

    /**
     * All user interactions that can happen in the Safety Hub dashboard, excluding the permissions
     * and notifications review modules. Must be kept in sync with SafetyHubDashboardInteractions in
     * settings/enums.xml.
     */
    @IntDef({
        DashboardInteractions.OPEN_PLAY_STORE,
        DashboardInteractions.GO_TO_SAFE_BROWSING_SETTINGS,
        DashboardInteractions.OPEN_SAFETY_TOOLS_INFO,
        DashboardInteractions.OPEN_INCOGNITO_INFO,
        DashboardInteractions.OPEN_SAFE_BROWSING_INFO,
        DashboardInteractions.OPEN_HELP_CENTER,
        DashboardInteractions.OPEN_PASSWORD_MANAGER,
        DashboardInteractions.SHOW_SIGN_IN_PROMO,
        DashboardInteractions.MAX_VALUE
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface DashboardInteractions {
        int OPEN_PLAY_STORE = 0;
        int GO_TO_SAFE_BROWSING_SETTINGS = 1;
        int OPEN_SAFETY_TOOLS_INFO = 2;
        int OPEN_INCOGNITO_INFO = 3;
        int OPEN_SAFE_BROWSING_INFO = 4;
        int OPEN_HELP_CENTER = 5;
        int OPEN_PASSWORD_MANAGER = 6;
        int SHOW_SIGN_IN_PROMO = 7;
        int MAX_VALUE = SHOW_SIGN_IN_PROMO;
    }

    /**
     * State for a Safety Hub module. Must be kept in sync with SafetyHubModuleState in
     * settings/enums.xml.
     */
    @IntDef({
        ModuleStateEnum.WARNING,
        ModuleStateEnum.UNAVAILABLE,
        ModuleStateEnum.INFO,
        ModuleStateEnum.SAFE,
        ModuleStateEnum.LOADING
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ModuleStateEnum {
        int WARNING = 0;
        int UNAVAILABLE = 1;
        int INFO = 2;
        int SAFE = 3;
        int LOADING = 4;
        int MAX_VALUE = LOADING;
    }

    public static String getDashboardModuleTypeForModuleOption(@ModuleOption int option) {
        switch (option) {
            case ModuleOption.UPDATE_CHECK:
                return DashboardModuleType.UPDATE_CHECK;
            case ModuleOption.ACCOUNT_PASSWORDS:
                return DashboardModuleType.ACCOUNT_PASSWORDS;
            case ModuleOption.LOCAL_PASSWORDS:
                return DashboardModuleType.LOCAL_PASSWORDS;
            case ModuleOption.UNIFIED_PASSWORDS:
                return DashboardModuleType.UNIFIED_PASSWORDS;
            case ModuleOption.SAFE_BROWSING:
                return DashboardModuleType.SAFE_BROWSING;
            case ModuleOption.UNUSED_PERMISSIONS:
                return DashboardModuleType.REVOKED_PERMISSIONS;
            case ModuleOption.NOTIFICATION_REVIEW:
                return DashboardModuleType.NOTIFICATION_REVIEW;
            default:
                throw new IllegalArgumentException();
        }
    }

    public static void recordExternalInteractions(@ExternalInteractions int value) {
        RecordHistogram.recordEnumeratedHistogram(
                EXTERNAL_INTERACTIONS_HISTOGRAM_NAME, value, ExternalInteractions.MAX_VALUE);
    }

    static @ModuleStateEnum int getModuleStateEnum(@ModuleState int state) {
        switch (state) {
            case ModuleState.WARNING:
                return ModuleStateEnum.WARNING;
            case ModuleState.UNAVAILABLE:
                return ModuleStateEnum.UNAVAILABLE;
            case ModuleState.INFO:
                return ModuleStateEnum.INFO;
            case ModuleState.SAFE:
                return ModuleStateEnum.SAFE;
            case ModuleState.LOADING:
                return ModuleStateEnum.LOADING;
            default:
                throw new IllegalArgumentException();
        }
    }

    static void recordModuleState(
            @ModuleState int state,
            @DashboardModuleType String moduleType,
            @LifecycleEvent String lifecycleEvent) {
        StringJoiner joiner = new StringJoiner(".");
        joiner.add(MODULE_STATE_HISTOGRAM_NAME);
        joiner.add(moduleType);
        joiner.add(lifecycleEvent);
        String histogramName = joiner.toString();

        RecordHistogram.recordEnumeratedHistogram(
                histogramName, getModuleStateEnum(state), ModuleStateEnum.MAX_VALUE);
    }

    static void recordRevokedPermissionsInteraction(@PermissionsModuleInteractions int value) {
        RecordHistogram.recordEnumeratedHistogram(
                PERMISSIONS_INTERACTIONS_HISTOGRAM_NAME,
                value,
                PermissionsModuleInteractions.MAX_VALUE);
    }

    static void recordNotificationsInteraction(@NotificationsModuleInteractions int value) {
        RecordHistogram.recordEnumeratedHistogram(
                NOTIFICATIONS_INTERACTIONS_HISTOGRAM_NAME,
                value,
                NotificationsModuleInteractions.MAX_VALUE);
    }

    static void recordDashboardInteractions(@DashboardInteractions int value) {
        RecordHistogram.recordEnumeratedHistogram(
                DASHBOARD_INTERACTIONS_HISTOGRAM_NAME, value, DashboardInteractions.MAX_VALUE);
    }

    static void maybeRecordAbusiveNotificationRevokedInteraction(
            PermissionsData @Nullable [] permissionsDataList,
            @PermissionsModuleInteractions int value) {
        if (permissionsDataList == null) return;

        // If any of the `PermissionsData` objects include notifications, log the histogram once.
        for (PermissionsData permissionsData : permissionsDataList) {
            switch (permissionsData.getRevocationType()) {
                case PermissionsRevocationType.ABUSIVE_NOTIFICATION_PERMISSIONS:
                case PermissionsRevocationType.UNUSED_PERMISSIONS_AND_ABUSIVE_NOTIFICATIONS:
                case PermissionsRevocationType.SUSPICIOUS_NOTIFICATION_PERMISSIONS:
                case PermissionsRevocationType.UNUSED_PERMISSIONS_AND_SUSPICIOUS_NOTIFICATIONS:
                    RecordHistogram.recordEnumeratedHistogram(
                            ABUSIVE_NOTIFICATION_REVOCATION_INTERACTIONS_HISTOGRAM_NAME,
                            value,
                            PermissionsModuleInteractions.MAX_VALUE);
                    return;
            }
        }
    }
}
