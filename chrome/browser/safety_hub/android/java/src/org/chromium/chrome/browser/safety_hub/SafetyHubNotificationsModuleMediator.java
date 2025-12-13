// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordNotificationsInteraction;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.NotificationsModuleInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleOption;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/**
 * Mediator for the Safety Hub notifications module. Populates the {@link
 * SafetyHubExpandablePreference} with the notification permissions review state. It also listens to
 * changes of this service, and updates the preference to reflect these.
 */
@NullMarked
public class SafetyHubNotificationsModuleMediator
        implements SafetyHubModuleMediator, NotificationPermissionReviewBridge.Observer {
    private final NotificationPermissionReviewBridge mNotificationPermissionReviewBridge;
    private final SafetyHubExpandablePreference mPreference;
    private final SafetyHubModuleMediatorDelegate mDelegate;

    private PropertyModel mModel;
    private int mNotificationPermissionsForReviewCount;

    SafetyHubNotificationsModuleMediator(
            SafetyHubExpandablePreference preference,
            SafetyHubModuleMediatorDelegate delegate,
            NotificationPermissionReviewBridge notificationPermissionReviewBridge) {
        mPreference = preference;
        mNotificationPermissionReviewBridge = notificationPermissionReviewBridge;
        mDelegate = delegate;
    }

    @Override
    public void setUpModule() {
        mModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.ALL_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .build();

        PropertyModelChangeProcessor.create(
                mModel, mPreference, SafetyHubModuleViewBinder::bindProperties);

        mNotificationPermissionReviewBridge.addObserver(this);
        mNotificationPermissionsForReviewCount = getNotificationsPermissionsCount();
    }

    @Override
    public void destroy() {
        mNotificationPermissionReviewBridge.removeObserver(this);
    }

    @Override
    public void notificationPermissionsChanged() {
        updateModule();
        mDelegate.onUpdateNeeded();
    }

    @Override
    public void updateModule() {
        mNotificationPermissionsForReviewCount = getNotificationsPermissionsCount();

        mModel.set(SafetyHubModuleProperties.TITLE, getTitle());
        mModel.set(SafetyHubModuleProperties.SUMMARY, getSummary());
        mModel.set(SafetyHubModuleProperties.PRIMARY_BUTTON_TEXT, getPrimaryButtonText());
        mModel.set(SafetyHubModuleProperties.SECONDARY_BUTTON_TEXT, getSecondaryButtonText());
        mModel.set(SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER, getPrimaryButtonListener());
        mModel.set(
                SafetyHubModuleProperties.SECONDARY_BUTTON_LISTENER, getSecondaryButtonListener());
        mModel.set(SafetyHubModuleProperties.ORDER, getOrder());
        mModel.set(SafetyHubModuleProperties.ICON, getIcon(mPreference.getContext()));
    }

    @Override
    public void setExpandState(boolean expanded) {
        mModel.set(SafetyHubModuleProperties.IS_EXPANDED, expanded);
    }

    @Override
    public @ModuleState int getModuleState() {
        return SafetyHubUtils.getNotificationModuleState(mNotificationPermissionsForReviewCount);
    }

    @Override
    public @ModuleOption int getOption() {
        return ModuleOption.NOTIFICATION_REVIEW;
    }

    @Override
    public boolean isManaged() {
        return false;
    }

    public int getNotificationsPermissionsCount() {
        assert mNotificationPermissionReviewBridge != null;
        return mNotificationPermissionReviewBridge.getNotificationPermissions().size();
    }

    private String getTitle() {
        if (mNotificationPermissionsForReviewCount == 0) {
            return mPreference
                    .getContext()
                    .getString(R.string.safety_hub_notifications_review_ok_title);
        }
        return mPreference
                .getContext()
                .getResources()
                .getQuantityString(
                        R.plurals.safety_hub_notifications_review_warning_title,
                        mNotificationPermissionsForReviewCount,
                        mNotificationPermissionsForReviewCount);
    }

    private String getSummary() {
        if (mNotificationPermissionsForReviewCount == 0) {
            return mPreference
                    .getContext()
                    .getString(R.string.safety_hub_notifications_review_ok_summary);
        }
        return mPreference
                .getContext()
                .getResources()
                .getQuantityString(
                        R.plurals.safety_hub_notifications_review_warning_summary,
                        mNotificationPermissionsForReviewCount);
    }

    private @Nullable String getPrimaryButtonText() {
        return mNotificationPermissionsForReviewCount == 0
                ? null
                : mPreference
                        .getContext()
                        .getString(R.string.safety_hub_notifications_reset_all_button);
    }

    private View.@Nullable OnClickListener getPrimaryButtonListener() {
        if (mNotificationPermissionsForReviewCount == 0) {
            return null;
        }
        return v -> {
            List<NotificationPermissions> notificationPermissionsList =
                    mNotificationPermissionReviewBridge.getNotificationPermissions();
            mNotificationPermissionReviewBridge.bulkResetNotificationPermissions();
            mDelegate.showSnackbarForModule(
                    mPreference
                            .getContext()
                            .getResources()
                            .getQuantityString(
                                    R.plurals.safety_hub_notifications_bulk_reset_snackbar,
                                    notificationPermissionsList.size(),
                                    notificationPermissionsList.size()),
                    Snackbar.UMA_SAFETY_HUB_MULTIPLE_SITE_NOTIFICATIONS,
                    new SnackbarManager.SnackbarController() {
                        @Override
                        public void onAction(@Nullable Object actionData) {
                            assert actionData != null : "Action data should be non-null.";
                            mNotificationPermissionReviewBridge.bulkAllowNotificationPermissions(
                                    (List<NotificationPermissions>) actionData);
                            recordNotificationsInteraction(
                                    NotificationsModuleInteractions.UNDO_BLOCK_ALL);
                        }
                    },
                    notificationPermissionsList);
            recordNotificationsInteraction(NotificationsModuleInteractions.BLOCK_ALL);
        };
    }

    private String getSecondaryButtonText() {
        if (mNotificationPermissionsForReviewCount == 0) {
            return mPreference
                    .getContext()
                    .getString(R.string.safety_hub_go_to_notification_settings_button);
        }
        return mPreference.getContext().getString(R.string.safety_hub_view_sites_button);
    }

    private View.OnClickListener getSecondaryButtonListener() {
        if (mNotificationPermissionsForReviewCount == 0) {
            return v -> {
                mDelegate.launchSiteSettingsActivityForModule(
                        SiteSettingsCategory.Type.NOTIFICATIONS);
                recordNotificationsInteraction(NotificationsModuleInteractions.GO_TO_SETTINGS);
            };
        }
        return v -> {
            mDelegate.startSettingsForModule(SafetyHubNotificationsFragment.class);
            recordNotificationsInteraction(NotificationsModuleInteractions.OPEN_UI_REVIEW);
        };
    }
}
