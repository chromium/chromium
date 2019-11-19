// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.content.Intent;

/**
 * Constants used in more than a single Notification class, e.g. intents and extra names.
 */
public class NotificationConstants {
    // These actions have to be synchronized with the receiver defined in AndroidManifest.xml.
    static final String ACTION_CLICK_NOTIFICATION =
            "org.chromium.chrome.browser.notifications.CLICK_NOTIFICATION";
    static final String ACTION_CLOSE_NOTIFICATION =
            "org.chromium.chrome.browser.notifications.CLOSE_NOTIFICATION";

    /**
     * Name of the Intent extra set by the framework when a notification preferences intent has
     * been triggered from there, which could be one of the setting gears in system UI.
     */
    static final String EXTRA_NOTIFICATION_TAG = "notification_tag";

    /**
     * Names of the Intent extras used for Intents related to notifications. These intents are set
     * and owned by Chromium.
     *
     * When adding a new extra, as well as setting it on the intent in NotificationPlatformBridge,
     * it *must* also be set in {@link NotificationJobService#getJobExtrasFromIntent(Intent)}
     */
    public static final String EXTRA_NOTIFICATION_ID = "notification_id";
    static final String EXTRA_NOTIFICATION_TYPE = "notification_type";
    static final String EXTRA_NOTIFICATION_INFO_ORIGIN = "notification_info_origin";
    static final String EXTRA_NOTIFICATION_INFO_SCOPE = "notification_info_scope";
    static final String EXTRA_NOTIFICATION_INFO_PROFILE_ID = "notification_info_profile_id";
    static final String EXTRA_NOTIFICATION_INFO_PROFILE_INCOGNITO =
            "notification_info_profile_incognito";
    static final String EXTRA_NOTIFICATION_INFO_ACTION_INDEX = "notification_info_action_index";
    static final String EXTRA_NOTIFICATION_INFO_WEBAPK_PACKAGE = "notification_info_webapk_package";
    static final String EXTRA_NOTIFICATION_REPLY = "notification_reply";
    static final String EXTRA_NOTIFICATION_ACTION = "notification_action";

    static final String EXTRA_JOB_SCHEDULED_TIME_MS = "notification_job_scheduled_time_ms";
    static final String EXTRA_JOB_STARTED_TIME_MS = "notification_job_started_time_ms";

    /**
     * Unique identifier for a single sync notification. Since the notification ID is reused,
     * old notifications will be overwritten.
     */
    public static final int NOTIFICATION_ID_SYNC = 1;
    /**
     * Unique identifier for the "Signed in to Chrome" notification.
     */
    @SuppressWarnings("unused")
    public static final int NOTIFICATION_ID_SIGNED_IN = 2;
    /**
     * Unique identifier for the Physical Web notification.
     */
    public static final int NOTIFICATION_ID_PHYSICAL_WEB = 3;

    /**
     * Unique identifier for Browser Actions notification.
     */
    public static final int NOTIFICATION_ID_BROWSER_ACTIONS = 4;

    /**
     * Unique identifier for standalone Web App actions notification.
     */
    public static final int NOTIFICATION_ID_WEBAPP_ACTIONS = 5;

    /**
     * Unique identifier for the persistent notification displayed while a Trusted Web Activity is
     * in foreground. No longer used.
     */
    @SuppressWarnings("unused")
    public static final int NOTIFICATION_ID_TWA_PERSISTENT = 6;

    /**
     * Unique identifier for notification shown in VR if Chrome's VR browser is still getting ready
     * and cannot be accessed yet.
     */
    public static final int NOTIFICATION_ID_PREPARING_VR = 7;

    /**
     * Unique identifier for the summary notification for downloads.  Using the ID this summary was
     * going to have before it was migrated here.
     * TODO(dtrainor): Clean up this ID and make sure it's in line with existing id counters without
     * tags.
     */
    public static final int NOTIFICATION_ID_DOWNLOAD_SUMMARY = 999999;

    /**
     * Unique identifier for a single update notification.
     */
    public static final int NOTIFICATION_ID_UPDATE = 8;

    /**
     * Unique identifier for ClickToCall notifications.
     */
    public static final int NOTIFICATION_ID_CLICK_TO_CALL = 9;

    /**
     * Unique identifier for Shared Clipboard incoming notifications.
     */
    public static final int NOTIFICATION_ID_SHARED_CLIPBOARD_INCOMING = 10;

    /**
     * Unique identifier for Shared Clipboard outgoing notifications.
     */
    public static final int NOTIFICATION_ID_SHARED_CLIPBOARD_OUTGOING = 11;

    /**
     * Unique identifier for ClickToCall error notification.
     */
    public static final int NOTIFICATION_ID_CLICK_TO_CALL_ERROR = 12;

    /**
     * Separator used to separate the notification origin from additional data such as the
     * developer specified tag. This and the prefix following it need to be the same as the one
     * specified in notification_id_generator.cc.
     */
    static final String NOTIFICATION_TAG_SEPARATOR = "#";
    static final String PERSISTENT_NOTIFICATION_TAG_PREFIX = "p";

    /**
     * Key for retrieving the results of user input from notification text action intents.
     */
    static final String KEY_TEXT_REPLY = "key_text_reply";

    // Notification groups for features that show notifications to the user.
    public static final String GROUP_DOWNLOADS = "Downloads";
    public static final String GROUP_INCOGNITO = "Incognito";
    public static final String GROUP_MEDIA_PLAYBACK = "MediaPlayback";
    public static final String GROUP_MEDIA_PRESENTATION = "MediaPresentation";
    public static final String GROUP_MEDIA_REMOTE = "MediaRemote";
    public static final String GROUP_SYNC = "Sync";
    public static final String GROUP_WEBAPK = "WebApk";
    public static final String GROUP_SEND_TAB_TO_SELF = "SendTabToSelf";
    public static final String GROUP_CLICK_TO_CALL = "ClickToCall";
    public static final String GROUP_SHARED_CLIPBOARD = "SharedClipboard";

    // Web notification group names are set dynamically as this prefix + notification origin.
    // For example, 'Web:chromium.org' for a notification from chromium.org.
    static final String GROUP_WEB_PREFIX = "Web:";

    // Default notificationId until it has been set.
    public static final int DEFAULT_NOTIFICATION_ID = -1;
}
